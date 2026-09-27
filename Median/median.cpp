
#include "avisynth.h"
#include "print.h"
#include "median.h"
#include "opt_med.h"
#include <vector>
#include "kernels.h"
#include <cstring>
#include <algorithm>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#define _CRT_SECURE_NO_WARNINGS
#endif
//////////////////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////////////////
Median::Median(PClip _child, std::vector<PClip> _clips, unsigned int _low, unsigned int _high, bool _temporal, bool _processchroma, unsigned int _sync, unsigned int _syncx, unsigned int _syncy, unsigned int _samples, unsigned int _ignoret, unsigned int _ignoreb, unsigned int _ignorel, unsigned int _ignorer, bool _debug, unsigned int _threads, IScriptEnvironment* env) :
  GenericVideoFilter(_child), clips(_clips), low(_low), high(_high), temporal(_temporal), processchroma(_processchroma), sync(_sync), syncx(_syncx), syncy(_syncy), samples(_samples), ignoret(_ignoret), ignoreb(_ignoreb), ignorel(_ignorel), ignorer(_ignorer), debug(_debug), threads(_threads)
{
  // Check frame property support
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  if (temporal)
    depth = 2 * low + 1; // In this case low == high == radius and we only have one source clip
  else
    depth = (int)clips.size();

  blend = depth - low - high;

  if (blend == 1 && low == high && depth <= MAX_OPT)
    fastprocess = true;
  else
    fastprocess = false;

#ifdef _WIN32
  debugf("depth: %d, blend: %d, low: %d, high: %d, fast: %d, temporal: %d, sync: %d, syncx: %d, syncy: %d, samples: %d",
    depth, blend, low, high, (int)fastprocess, (int)temporal, (int)sync, (int)syncx, (int)syncy, (int)samples);
#endif

  switch (depth)
  {
  case 3: fastmedian = opt_med3; break;
  case 5: fastmedian = opt_med5; break;
  case 7: fastmedian = opt_med7; break;
  case 9: fastmedian = opt_med9; break;
  }

  if (temporal)
  {
    info.push_back(clips[0]->GetVideoInfo());
  }
  else // When dealing with more than one source, make sure that they match
  {
    for (unsigned int i = 0; i < depth; i++)
      info.push_back(clips[i]->GetVideoInfo());

    for (unsigned int i = 1; i < depth; i++)
    {
      if (!info[i].IsSameColorspace(info[0]))
        env->ThrowError(ERROR_PREFIX "Format of all clips must match.");
    }

    for (unsigned int i = 1; i < depth; i++)
    {
      if (info[i].width != info[0].width || info[i].height != info[0].height)
        env->ThrowError(ERROR_PREFIX "Dimensions of all clips must match.");
    }
  }
}


//////////////////////////////////////////////////////////////////////////////
// Destructor
//////////////////////////////////////////////////////////////////////////////
Median::~Median()
{
}


//////////////////////////////////////////////////////////////////////////////
// Actual image processing operations
//////////////////////////////////////////////////////////////////////////////
PVideoFrame __stdcall Median::GetFrame(int n, IScriptEnvironment* env)
{
  // Sync statistics for this frame
  double best[MAX_DEPTH] = { 0.0 };
  int match[MAX_DEPTH] = { 0 };
  int match_x[MAX_DEPTH] = { 0 };
  int match_y[MAX_DEPTH] = { 0 };

  // Source
  PVideoFrame src[MAX_DEPTH];

  if (temporal)
  {
    unsigned int radius = low; // low == high == radius

    // TODO: Do I need to worry about negative frames or frames after the last? Looks like no
    for (unsigned int i = 0; i < depth; i++)
      src[i] = clips[0]->GetFrame(std::clamp(n - static_cast<int>(radius) + static_cast<int>(i), 0, vi.num_frames - 1), env); // Grab an equal number of preceding and following frames
  }
  else if (sync > 0 || syncx > 0 || syncy > 0)
  {
    src[0] = clips[0]->GetFrame(n, env);

    for (unsigned int i = 1; i < depth; i++)
    {
      int radius = sync;
      int rx = syncx;
      int ry = syncy;
      match_x[i] = 0;
      match_y[i] = 0;

      for (int j = -radius; j <= radius; j++)
      {
        PVideoFrame candidate = clips[i]->GetFrame(std::clamp(n + j, 0, info[i].num_frames - 1), env);
        for (int dy = -ry; dy <= ry; dy++)
        {
          for (int dx = -rx; dx <= rx; dx++)
          {
            double similarity = CompareFrames(PLANAR_Y, src[0], candidate, samples, dx, dy);
            similarity -= abs(j) * 0.1;

            if (similarity > best[i])
            {
              best[i] = similarity;
              match[i] = j;
              match_x[i] = dx;
              match_y[i] = dy;
            }
          }
        }
      }

      src[i] = clips[i]->GetFrame(std::clamp(n + match[i], 0, info[i].num_frames - 1), env);
    }
  }
  else
  {
    for (unsigned int i = 0; i < depth; i++)
      src[i] = clips[i]->GetFrame(n, env);
  }

  // Output
  // w/ frame property copy source
  PVideoFrame output = has_at_least_v8 ? env->NewVideoFrameP(vi, temporal ? &src[low] : &src[0]) : env->NewVideoFrame(vi);

  ProcessFrame(src, output, match_x, match_y, env);

  // Print debug information on output image
  if (debug)
  {
    unsigned int line = 0;
    textf(output, line, "FRAME: %d", n);
    textf(output, line, "CLIPS: %d", depth);

    if (sync > 0)
    {
      textf(output, line, "SYNC RADIUS: %d", sync);
      textf(output, line, "SYNC METRICS:");

      for (unsigned int i = 1; i < depth; i++)
        textf(output, line, "%-2d %+-3d %-f", i + 1, match[i], best[i]);
    }
  }

  return output;
}


//////////////////////////////////////////////////////////////////////////////
// Compare two frames
// 
// returns 100.0 -> exact match, 0.0 -> completely different
//////////////////////////////////////////////////////////////////////////////
double Median::CompareFrames(int plane, PVideoFrame a, PVideoFrame b, unsigned int points, int dx, int dy)
{
  int cs = info[0].ComponentSize();
  const int width = a->GetRowSize(plane) / cs;
  const int height = a->GetHeight(plane);
  const int pitch_a = a->GetPitch(plane) / cs;
  const int pitch_b = b->GetPitch(plane) / cs;

  int ig_t = (ignoret * height) / info[0].height;
  int ig_b = (ignoreb * height) / info[0].height;
  int ig_l = (ignorel * width) / info[0].width;
  int ig_r = (ignorer * width) / info[0].width;

  int start_x = std::max((int)ig_l, dx + (int)ig_l);
  int end_x = std::min(width - (int)ig_r, width + dx - (int)ig_r);
  int start_y = std::max((int)ig_t, dy + (int)ig_t);
  int end_y = std::min(height - (int)ig_b, height + dy - (int)ig_b);
  
  if (end_x <= start_x || end_y <= start_y) return 0.0;

  int overlap_width = end_x - start_x;
  int overlap_height = end_y - start_y;
  unsigned int overlap_length = overlap_width * overlap_height;

  if (points < 1 || points > overlap_length) points = overlap_length;
  const unsigned int step = overlap_length / points;

  double difference = 0.0;
  
  if (cs == 1) {
    const uint8_t* aptr = (const uint8_t*)a->GetReadPtr(plane);
    const uint8_t* bptr = (const uint8_t*)b->GetReadPtr(plane);
    unsigned long sum = 0;
    unsigned int sampled = 0;
    unsigned int count = 0;
    for (int y = start_y; y < end_y; ++y) {
      const uint8_t* row_a = aptr + y * pitch_a;
      const uint8_t* row_b = bptr + (y - dy) * pitch_b;
      for (int x = start_x; x < end_x; ++x) {
        if (count == 0) {
          sum += abs((int)row_a[x] - (int)row_b[x - dx]);
          sampled++;
          count = step;
        }
        count--;
      }
    }
    double max_val = (info[0].ComponentSize() == 4 ? 1 : (1 << info[0].BitsPerComponent()) - 1);
    difference = (100.0 * sum) / (max_val * (sampled > 0 ? sampled : 1));
  } else if (cs == 2) {
    const uint16_t* aptr = (const uint16_t*)a->GetReadPtr(plane);
    const uint16_t* bptr = (const uint16_t*)b->GetReadPtr(plane);
    unsigned long long sum = 0;
    unsigned int sampled = 0;
    unsigned int count = 0;
    for (int y = start_y; y < end_y; ++y) {
      const uint16_t* row_a = aptr + y * pitch_a;
      const uint16_t* row_b = bptr + (y - dy) * pitch_b;
      for (int x = start_x; x < end_x; ++x) {
        if (count == 0) {
          sum += abs((int)row_a[x] - (int)row_b[x - dx]);
          sampled++;
          count = step;
        }
        count--;
      }
    }
    double max_val = (info[0].ComponentSize() == 4 ? 1 : (1 << info[0].BitsPerComponent()) - 1);
    difference = (100.0 * sum) / (max_val * (sampled > 0 ? sampled : 1));
  } else if (cs == 4) {
    const float* aptr = (const float*)a->GetReadPtr(plane);
    const float* bptr = (const float*)b->GetReadPtr(plane);
    double sum = 0.0;
    unsigned int sampled = 0;
    unsigned int count = 0;
    for (int y = start_y; y < end_y; ++y) {
      const float* row_a = aptr + y * pitch_a;
      const float* row_b = bptr + (y - dy) * pitch_b;
      for (int x = start_x; x < end_x; ++x) {
        if (count == 0) {
          sum += std::abs(row_a[x] - row_b[x - dx]);
          sampled++;
          count = step;
        }
        count--;
      }
    }
    difference = (100.0 * sum) / (1.0 * (sampled > 0 ? sampled : 1));
  }

  double penalty = (abs(dx) * 0.1) + (abs(dy) * 0.1);
  return 100.0 - difference - penalty;
}


int __stdcall Median::SetCacheHints(int cachehints, int)
{
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

namespace {
AVSValue MedianWorker(IScriptEnvironment2*, void* data)
{
  median::process_plane(*static_cast<median::PlaneJob*>(data));
  return AVSValue();
}
}

void Median::ProcessFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst,
                          const int* match_x, const int* match_y, IScriptEnvironment* env)
{
  // Get the actual extended interface, rather than downcasting a legacy environment.
  PNeoEnv neo(env);
  IScriptEnvironment2* env2 = !neo ? nullptr : static_cast<IScriptEnvironment2*>(neo);
  unsigned int workers = 1;
  if (env2 && threads != 1) {
    const unsigned int pool = static_cast<unsigned int>(env->GetEnvProperty(AEP_THREADPOOL_THREADS));
    workers = threads == 0 ? std::max(1U, pool) : std::max(1U, std::min(threads, pool));
  }

  const bool planar = vi.IsPlanar();
  const bool rgb = vi.IsRGB();
  const int yuv_planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  const int rgb_planes[] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  const int* planes = rgb ? rgb_planes : yuv_planes;
  const int plane_count = planar ? vi.NumComponents() : 1;
  std::vector<median::PlaneJob> jobs;
  for (int p = 0; p < plane_count; ++p) {
    const int plane = planar ? planes[p] : 0;
    const bool copy = planar && !processchroma && (p == 3 || (!rgb && p > 0));
    // Acquire frame pointers only on the calling thread; jobs own raw, disjoint rows.
    uint8_t* output = dst->GetWritePtr(plane);
    if (copy) {
      env->BitBlt(output, dst->GetPitch(plane), src[0]->GetReadPtr(plane),
                  src[0]->GetPitch(plane), dst->GetRowSize(plane), dst->GetHeight(plane));
      continue;
    }
    median::PlaneJob job{};
    job.dst = output;
    job.dst_pitch = dst->GetPitch(plane);
    job.width = dst->GetRowSize(plane) / vi.ComponentSize();
    job.height = dst->GetHeight(plane);
    job.component_size = vi.ComponentSize();
    job.components = planar ? 1 : (vi.IsYUY2() ? 2 : vi.NumComponents());
    job.copy_every = !planar && !processchroma ? (vi.IsYUY2() ? 2 : (job.components == 4 ? 4 : 0)) : 0;
    job.depth = depth;
    job.low = low;
    job.high = high;
    for (unsigned int i = 0; i < depth; ++i) {
      job.src[i] = src[i]->GetReadPtr(plane);
      job.src_pitch[i] = src[i]->GetPitch(plane);
      // Scale signed offsets to the chroma plane's resolution (truncate toward zero).
      job.dx[i] = match_x[i] / (vi.width / (job.width / job.components));
      job.dy[i] = match_y[i] / (vi.height / job.height);
    }
    const unsigned int count = std::min(workers, std::max(1U, static_cast<unsigned int>(job.height / 32)));
    for (unsigned int t = 0; t < count; ++t) {
      job.start_y = static_cast<int>((static_cast<int64_t>(job.height) * t) / count);
      job.end_y = static_cast<int>((static_cast<int64_t>(job.height) * (t + 1)) / count);
      jobs.push_back(job);
    }
  }
  if (workers == 1 || jobs.size() <= 1) {
    for (const auto& job : jobs) median::process_plane(job);
    return;
  }
  IJobCompletion* completion = env2->NewCompletion(jobs.size() - 1);
  try {
    for (size_t i = 0; i + 1 < jobs.size(); ++i)
      env2->ParallelJob(MedianWorker, &jobs[i], completion);
    median::process_plane(jobs.back());
    completion->Wait();
  } catch (...) {
    // Never let submitted workers outlive their frame pointers or job descriptions.
    completion->Wait();
    completion->Destroy();
    throw;
  }
  completion->Destroy();
}


#ifdef _WIN32
//////////////////////////////////////////////////////////////////////////////
// Print things to be viewed in DebugView
//////////////////////////////////////////////////////////////////////////////
void Median::debugf(const char* fmt, ...)
{
  if (debug)
  {
    char buffer[1024] = { "median: " };
    char* ptr = buffer + strlen(buffer);

    va_list args;
    va_start(args, fmt);
    vsnprintf(ptr, sizeof(buffer), fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
  }
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Print things on top of image
//////////////////////////////////////////////////////////////////////////////
void Median::textf(PVideoFrame& dst, unsigned int& line, const char* fmt, ...)
{
  char string[1024] = { 0 };

  va_list args;
  va_start(args, fmt);
  vsnprintf(string, sizeof(string), fmt, args);
  va_end(args);

  if (info[0].IsYUY2()) print_yuyv(dst, line, string);
  else if (info[0].IsRGB24()) print_rgb(dst, line, string, false, 1);
  else if (info[0].IsRGB32()) print_rgb(dst, line, string, true, 1);
  else if (info[0].IsRGB48()) print_rgb(dst, line, string, false, 2);
  else if (info[0].IsRGB64()) print_rgb(dst, line, string, true, 2);
  else if (info[0].IsPlanar()) print_planar(dst, line, string, info[0].ComponentSize(), (info[0].ComponentSize() == 4 ? 1 : (1 << info[0].BitsPerComponent()) - 1));

  line++;
}
