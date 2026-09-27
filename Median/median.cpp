
#include "avisynth.h"
#include "print.h"
#include "median.h"
#include "opt_med.h"
#include <vector>
#include <thread>
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
  GenericVideoFilter(_child), clips(_clips), low(_low), high(_high), temporal(_temporal), processchroma(_processchroma), sync(_sync), syncx(_syncx), syncy(_syncy), samples(_samples), ignoret(_ignoret), ignoreb(_ignoreb), ignorel(_ignorel), ignorer(_ignorer), debug(_debug), threads(std::max(1U, _threads))
{
  for (int i = 0; i < MAX_DEPTH; ++i) {
    match_x[i] = 0;
    match_y[i] = 0;
  }
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

  // Source
  PVideoFrame src[MAX_DEPTH];

  if (temporal)
  {
    unsigned int radius = low; // low == high == radius

    // TODO: Do I need to worry about negative frames or frames after the last? Looks like no
    for (unsigned int i = 0; i < depth; i++)
      src[i] = clips[0]->GetFrame(n - radius + i, env); // Grab an equal number of preceding and following frames
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
        for (int dy = -ry; dy <= ry; dy++)
        {
          for (int dx = -rx; dx <= rx; dx++)
          {
            double similarity = CompareFrames(PLANAR_Y, src[0], clips[i]->GetFrame(n + j, env), samples, dx, dy);
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

      src[i] = clips[i]->GetFrame(n + match[i], env);
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

  // Select between planar and interleaved processing
  if (info[0].IsPlanar())
    ProcessPlanarFrame(src, output, env);
  else
    ProcessInterleavedFrame(src, output, env);

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
    double max_val = (1 << info[0].BitsPerComponent()) - 1;
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
    double max_val = (1 << info[0].BitsPerComponent()) - 1;
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


//////////////////////////////////////////////////////////////////////////////
// Image processing for planar images
//////////////////////////////////////////////////////////////////////////////

void Median::ProcessPlanarFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env)
{
  int cs = info[0].ComponentSize();
  if (cs == 1) {
    ProcessPlane_T<uint8_t>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<uint8_t>(PLANAR_U, src, dst, env);
      ProcessPlane_T<uint8_t>(PLANAR_V, src, dst, env);
    }
  } else if (cs == 2) {
    ProcessPlane_T<uint16_t>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<uint16_t>(PLANAR_U, src, dst, env);
      ProcessPlane_T<uint16_t>(PLANAR_V, src, dst, env);
    }
  } else if (cs == 4) {
    ProcessPlane_T<float>(PLANAR_Y, src, dst, env);
    if (!info[0].IsY()) {
      ProcessPlane_T<float>(PLANAR_U, src, dst, env);
      ProcessPlane_T<float>(PLANAR_V, src, dst, env);
    }
  }
}

AVSValue __stdcall MedianWorker(IScriptEnvironment2* env, void* data) {
  MedianJobData* job = (MedianJobData*)data;
  
  if (!job->is_interleaved) {
    int plane = job->plane;
    int cs = job->component_size;
    const int width = job->src[0]->GetRowSize(plane) / cs;
    const int pitch = job->src[0]->GetPitch(plane) / cs;
    const int dst_pitch = job->dst[0]->GetPitch(plane) / cs;
    
    if (cs == 1) {
      uint8_t* dstp = (uint8_t*)(job->dst[0]->GetWritePtr(plane)) + job->start_y * dst_pitch;
      const uint8_t* read_ptrs[MAX_DEPTH];
      for (unsigned int i = 0; i < job->filter->depth; i++) read_ptrs[i] = (const uint8_t*)(job->src[i]->GetReadPtr(plane));
      
      for (int y = job->start_y; y < job->end_y; ++y) {
        for (int x = 0; x < width; ++x) {
          uint8_t values[MAX_DEPTH];
          for (unsigned int i = 0; i < job->filter->depth; i++) {
            int cy = std::max(0, std::min(job->src[i]->GetHeight(plane) - 1, y - job->filter->match_y[i]));
            int cx = std::max(0, std::min(width - 1, x - job->filter->match_x[i]));
            values[i] = read_ptrs[i][cy * pitch + cx];
          }
          if (plane == PLANAR_Y || job->filter->processchroma) dstp[x] = job->filter->ProcessPixel(values);
          else dstp[x] = values[0];
        }
        dstp += dst_pitch;
      }
    } else if (cs == 2) {
      uint16_t* dstp = (uint16_t*)(job->dst[0]->GetWritePtr(plane)) + job->start_y * dst_pitch;
      const uint16_t* read_ptrs[MAX_DEPTH];
      for (unsigned int i = 0; i < job->filter->depth; i++) read_ptrs[i] = (const uint16_t*)(job->src[i]->GetReadPtr(plane));
      
      for (int y = job->start_y; y < job->end_y; ++y) {
        for (int x = 0; x < width; ++x) {
          uint16_t values[MAX_DEPTH];
          for (unsigned int i = 0; i < job->filter->depth; i++) {
            int cy = std::max(0, std::min(job->src[i]->GetHeight(plane) - 1, y - job->filter->match_y[i]));
            int cx = std::max(0, std::min(width - 1, x - job->filter->match_x[i]));
            values[i] = read_ptrs[i][cy * pitch + cx];
          }
          if (plane == PLANAR_Y || job->filter->processchroma) dstp[x] = job->filter->template ProcessPixel_T<uint16_t>(values);
          else dstp[x] = values[0];
        }
        dstp += dst_pitch;
      }
    } else if (cs == 4) {
      float* dstp = (float*)(job->dst[0]->GetWritePtr(plane)) + job->start_y * dst_pitch;
      const float* read_ptrs[MAX_DEPTH];
      for (unsigned int i = 0; i < job->filter->depth; i++) read_ptrs[i] = (const float*)(job->src[i]->GetReadPtr(plane));
      
      for (int y = job->start_y; y < job->end_y; ++y) {
        for (int x = 0; x < width; ++x) {
          float values[MAX_DEPTH];
          for (unsigned int i = 0; i < job->filter->depth; i++) {
            int cy = std::max(0, std::min(job->src[i]->GetHeight(plane) - 1, y - job->filter->match_y[i]));
            int cx = std::max(0, std::min(width - 1, x - job->filter->match_x[i]));
            values[i] = read_ptrs[i][cy * pitch + cx];
          }
          if (plane == PLANAR_Y || job->filter->processchroma) dstp[x] = job->filter->template ProcessPixel_T<float>(values);
          else dstp[x] = values[0];
        }
        dstp += dst_pitch;
      }
    }
  } else {
    // Interleaved format (simplified to standard non-threaded for now, or you can thread it similarly)
    // To thread interleaved, we need more logic. I will implement single threaded for interleaved for brevity.
  }
  
  return AVSValue();
}

template<typename T>
void Median::ProcessPlane_T(int plane, PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env)
{
  const int height = src[0]->GetHeight(plane);
  
  IScriptEnvironment2* env2 = nullptr;
  try { env->CheckVersion(6); env2 = static_cast<IScriptEnvironment2*>(env); } catch (...) { }

  if (env2 && threads > 1) {
    IJobCompletion* completion = env2->NewCompletion(threads);
    std::vector<MedianJobData> jobs(threads);
    int chunk = height / threads;
    for (unsigned int t = 0; t < threads; ++t) {
      jobs[t].filter = this;
      jobs[t].plane = plane;
      jobs[t].src = src;
      jobs[t].dst = &dst;
      jobs[t].start_y = t * chunk;
      jobs[t].end_y = (t == threads - 1) ? height : (t + 1) * chunk;
      jobs[t].component_size = sizeof(T);
      jobs[t].is_interleaved = false;
      env2->ParallelJob(MedianWorker, &jobs[t], completion);
    }
    completion->Wait();
    completion->Destroy();
  } else {
    MedianJobData job;
    job.filter = this;
    job.plane = plane;
    job.src = src;
    job.dst = &dst;
    job.start_y = 0;
    job.end_y = height;
    job.component_size = sizeof(T);
    job.is_interleaved = false;
    MedianWorker(nullptr, &job);
  }
}

void Median::ProcessInterleavedFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst, IScriptEnvironment* env)
{
  // Just keeping the old interleaved code, no threads for interleaved since we focus on planar bit-depth support
  // The original interleaved processing logic:
  const unsigned char* read_ptrs[MAX_DEPTH];
  for (unsigned int i = 0; i < depth; i++) read_ptrs[i] = src[i]->GetReadPtr();
  unsigned char* dstp = dst->GetWritePtr();
  const int width = info[0].width;
  const int height = info[0].height;

  if (info[0].IsYUY2()) {
    unsigned char luma[MAX_DEPTH];
    unsigned char chroma[MAX_DEPTH];
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; x++) {
        for (unsigned int i = 0; i < depth; i++) {
          int cy = std::max(0, std::min(height - 1, y - match_y[i]));
          int cx = std::max(0, std::min(width - 1, x - match_x[i]));
          int pitch = src[i]->GetPitch();
          luma[i] = read_ptrs[i][cy * pitch + cx * 2];
          chroma[i] = read_ptrs[i][cy * pitch + cx * 2 + 1];
        }
        dstp[x * 2] = ProcessPixel(luma);
        dstp[x * 2 + 1] = processchroma ? ProcessPixel(chroma) : chroma[0];
      }
      dstp += dst->GetPitch();
    }
  } else if (info[0].IsRGB24()) {
    unsigned char b[MAX_DEPTH], g[MAX_DEPTH], r[MAX_DEPTH];
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; x++) {
        for (unsigned int i = 0; i < depth; i++) {
          int cy = std::max(0, std::min(height - 1, y - match_y[i]));
          int cx = std::max(0, std::min(width - 1, x - match_x[i]));
          int pitch = src[i]->GetPitch();
          b[i] = read_ptrs[i][cy * pitch + cx * 3]; 
          g[i] = read_ptrs[i][cy * pitch + cx * 3 + 1]; 
          r[i] = read_ptrs[i][cy * pitch + cx * 3 + 2];
        }
        dstp[x * 3] = ProcessPixel(b); dstp[x * 3 + 1] = ProcessPixel(g); dstp[x * 3 + 2] = ProcessPixel(r);
      }
      dstp += dst->GetPitch();
    }
  } else if (info[0].IsRGB32()) {
    unsigned char b[MAX_DEPTH], g[MAX_DEPTH], r[MAX_DEPTH], a[MAX_DEPTH];
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; x++) {
        for (unsigned int i = 0; i < depth; i++) {
          int cy = std::max(0, std::min(height - 1, y - match_y[i]));
          int cx = std::max(0, std::min(width - 1, x - match_x[i]));
          int pitch = src[i]->GetPitch();
          b[i] = read_ptrs[i][cy * pitch + cx * 4]; 
          g[i] = read_ptrs[i][cy * pitch + cx * 4 + 1]; 
          r[i] = read_ptrs[i][cy * pitch + cx * 4 + 2]; 
          a[i] = read_ptrs[i][cy * pitch + cx * 4 + 3];
        }
        dstp[x * 4] = ProcessPixel(b); dstp[x * 4 + 1] = ProcessPixel(g); dstp[x * 4 + 2] = ProcessPixel(r); dstp[x * 4 + 3] = processchroma ? ProcessPixel(a) : a[0];
      }
      dstp += dst->GetPitch();
    }
  } else if (info[0].pixel_type == VideoInfo::CS_BGR64) {
    uint16_t b_16bit[MAX_DEPTH], g_16bit[MAX_DEPTH], r_16bit[MAX_DEPTH], a_16bit[MAX_DEPTH];
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; x++) {
        for (unsigned int i = 0; i < depth; i++) {
          int cy = std::max(0, std::min(height - 1, y - match_y[i]));
          int cx = std::max(0, std::min(width - 1, x - match_x[i]));
          int pitch = src[i]->GetPitch();
          b_16bit[i] = read_ptrs[i][cy * pitch + cx * 8 + 0] | (read_ptrs[i][cy * pitch + cx * 8 + 1] << 8);
          g_16bit[i] = read_ptrs[i][cy * pitch + cx * 8 + 2] | (read_ptrs[i][cy * pitch + cx * 8 + 3] << 8);
          r_16bit[i] = read_ptrs[i][cy * pitch + cx * 8 + 4] | (read_ptrs[i][cy * pitch + cx * 8 + 5] << 8);
          a_16bit[i] = read_ptrs[i][cy * pitch + cx * 8 + 6] | (read_ptrs[i][cy * pitch + cx * 8 + 7] << 8);
        }
        uint16_t median_b = ProcessPixel_16bit(b_16bit);
        uint16_t median_g = ProcessPixel_16bit(g_16bit);
        uint16_t median_r = ProcessPixel_16bit(r_16bit);
        uint16_t median_a = ProcessPixel_16bit(a_16bit);
        dstp[x * 8] = static_cast<uint8_t>(median_b); dstp[x * 8 + 1] = static_cast<uint8_t>(median_b >> 8);
        dstp[x * 8 + 2] = static_cast<uint8_t>(median_g); dstp[x * 8 + 3] = static_cast<uint8_t>(median_g >> 8);
        dstp[x * 8 + 4] = static_cast<uint8_t>(median_r); dstp[x * 8 + 5] = static_cast<uint8_t>(median_r >> 8);
        dstp[x * 8 + 6] = static_cast<uint8_t>(processchroma ? median_a : a_16bit[0]); dstp[x * 8 + 7] = static_cast<uint8_t>(processchroma ? median_a >> 8 : a_16bit[0] >> 8);
      }
      dstp += dst->GetPitch();
    }
  }
}

// Processing of a stack of pixel values

// Processing of a stack of pixel values
//////////////////////////////////////////////////////////////////////////////

template<typename T>
inline T Median::ProcessPixel_T(T* values) const
{
  T output;
  unsigned int sum = 0;
  if (blend != depth) std::sort(values, values + depth);
  for (unsigned int i = low; i < low + blend; i++) sum += values[i];
  output = sum / blend;
  return output;
}

inline uint16_t Median::ProcessPixel_16bit(uint16_t* values) const
{
  uint16_t output;

  unsigned int sum = 0;

  if (blend != depth) // If all clips are to be blended, there is no need to sort them
    std::sort(values, values + depth);

  for (unsigned int i = low; i < low + blend; i++)
    sum = sum + values[i];

  output = sum / blend;

  return output;
}

inline unsigned char Median::ProcessPixel(unsigned char* values) const
{
  unsigned char output;

  if (fastprocess) // Can use a fast method
  {
    output = fastmedian(values);
  }
  else // Full processing
  {
    unsigned int sum = 0;

    if (blend != depth) // If all clips are to be blended, there is no need to sort them
      std::sort(values, values + depth);

    for (unsigned int i = low; i < low + blend; i++)
      sum = sum + values[i];

    output = sum / blend;
  }

  return output;
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
  else if (info[0].IsPlanar()) print_planar(dst, line, string, info[0].ComponentSize(), (1 << info[0].BitsPerComponent()) - 1);

  line++;
}
