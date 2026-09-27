//////////////////////////////////////////////////////////////////////////////
// Median filter for AviSynth
//
// This filter will take in a number of clips and calculate a pixel-by-pixel
// median out of them. This is useful for reducing noise and glitches in
// analog tape captures, but may have other uses as well.
//
// Author: antti.korhola@gmail.com
//
// License: Public domain. Credit would be nice, but do with this what you will.
//
//////////////////////////////////////////////////////////////////////////////

// Includes
#include "avisynth.h"
#include <vector>
#include <stdint.h>
#include "median.h"

//////////////////////////////////////////////////////////////////////////////
// Create Median filter
//////////////////////////////////////////////////////////////////////////////
AVSValue __cdecl Create_Median(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  AVSValue array = args[0];
  int n = array.ArraySize();

  if (n < 3 || n > 25 || n % 2 == 0)
    env->ThrowError(ERROR_PREFIX "Need an odd number of clips between 3 and 25.");

  std::vector<PClip> clips;

  for (int i = 0; i < n; i++)
    clips.push_back(array[i].AsClip());

  // Parameters
  bool chroma = args[1].AsBool(true);
  int sync = args[2].AsInt(0);
  int syncx = args[3].AsInt(0);
  int syncy = args[4].AsInt(0);
  int ignoret = args[5].AsInt(0);
  int ignoreb = args[6].AsInt(0);
  int ignorel = args[7].AsInt(0);
  int ignorer = args[8].AsInt(0);
  int samples = args[9].AsInt(4096U);
  bool debug = args[10].AsBool(false);
  int threads = args[11].AsInt(1);

  int opt = args[12].AsInt(0);

  bool opencl = args[13].AsBool(false);

  const char* device_type = args[14].AsString("auto");
  int device_id = args[15].AsInt(0);

  // Validation
  if (opt < 0 || opt > 8)
    env->ThrowError(ERROR_PREFIX "Opt must be between 0 and 8.");
  if (threads < 0)
    env->ThrowError(ERROR_PREFIX "Threads must be zero (automatic) or positive.");

  if (syncx < 0 || syncy < 0 || ignoret < 0 || ignoreb < 0 || ignorel < 0 || ignorer < 0)
    env->ThrowError(ERROR_PREFIX "Sync radii and border exclusions must be non-negative.");

  if (sync < 0)
    env->ThrowError(ERROR_PREFIX "Sync needs to be a positive value.");

  if (samples < 0)
    env->ThrowError(ERROR_PREFIX "Samples needs to be a positive value.");

  // Set low and high so that a regular median function is achieved
  unsigned int limit = (n - 1) / 2;

  return new Median(clips[0], clips, limit, limit, false, chroma, sync, syncx, syncy, samples, ignoret, ignoreb, ignorel, ignorer, debug, threads, opt, opencl, device_type, device_id, env);
}


//////////////////////////////////////////////////////////////////////////////
// Create TemporalMedian filter
//////////////////////////////////////////////////////////////////////////////
AVSValue __cdecl Create_TemporalMedian(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  std::vector<PClip> clips;
  clips.push_back(args[0].AsClip());

  // Parameters
  int radius = args[1].AsInt(1);
  bool chroma = args[2].AsBool(true);
  bool debug = args[3].AsBool(false);
  int threads = args[4].AsInt(1);

  int opt = args[5].AsInt(0);

  bool opencl = args[6].AsBool(false);

  const char* device_type = args[7].AsString("auto");
  int device_id = args[8].AsInt(0);

  // Validation
  if (opt < 0 || opt > 8)
    env->ThrowError(ERROR_PREFIX "Opt must be between 0 and 8.");
  if (threads < 0)
    env->ThrowError(ERROR_PREFIX "Threads must be zero (automatic) or positive.");

  if (radius < 1 || radius > 12)
    env->ThrowError(ERROR_PREFIX "Radius needs to be between 1 and 12.");

  return new Median(clips[0], clips, radius, radius, true, chroma, 0, 0, 0, 0, 0, 0, 0, 0, debug, threads, opt, opencl, device_type, device_id, env);
}


//////////////////////////////////////////////////////////////////////////////
// Create MedianBlend filter
//////////////////////////////////////////////////////////////////////////////
AVSValue __cdecl Create_MedianBlend(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  AVSValue array = args[0];
  int n = array.ArraySize();

  if (n < 3 || n > 25)
    env->ThrowError(ERROR_PREFIX "Need 3-25 clips.");

  std::vector<PClip> clips;

  for (int i = 0; i < n; i++)
    clips.push_back(array[i].AsClip());

  // Parameters
  int low = args[1].AsInt(1);
  int high = args[2].AsInt(1);
  bool chroma = args[3].AsBool(true);
  int sync = args[4].AsInt(0);
  int syncx = args[5].AsInt(0);
  int syncy = args[6].AsInt(0);
  int ignoret = args[7].AsInt(0);
  int ignoreb = args[8].AsInt(0);
  int ignorel = args[9].AsInt(0);
  int ignorer = args[10].AsInt(0);
  int samples = args[11].AsInt(4096U);
  bool debug = args[12].AsBool(false);
  int threads = args[13].AsInt(1);

  int opt = args[14].AsInt(0);

  bool opencl = args[15].AsBool(false);

  const char* device_type = args[16].AsString("auto");
  int device_id = args[17].AsInt(0);

  // Validation
  if (opt < 0 || opt > 8)
    env->ThrowError(ERROR_PREFIX "Opt must be between 0 and 8.");
  if (threads < 0)
    env->ThrowError(ERROR_PREFIX "Threads must be zero (automatic) or positive.");

  if (low < 0 || high < 0 || low >= n || high >= n || low + high >= n)
    env->ThrowError(ERROR_PREFIX "Invalid values supplied for low and/or high limits.");

  if (syncx < 0 || syncy < 0 || ignoret < 0 || ignoreb < 0 || ignorel < 0 || ignorer < 0)
    env->ThrowError(ERROR_PREFIX "Sync radii and border exclusions must be non-negative.");

  if (sync < 0)
    env->ThrowError(ERROR_PREFIX "Sync needs to be a positive value.");

  if (samples < 0)
    env->ThrowError(ERROR_PREFIX "Samples needs to be a positive value.");

  return new Median(clips[0], clips, low, high, false, chroma, sync, syncx, syncy, samples, ignoret, ignoreb, ignorel, ignorer, debug, threads, opt, opencl, device_type, device_id, env);
}


//////////////////////////////////////////////////////////////////////////////
// Add filters
//////////////////////////////////////////////////////////////////////////////
const AVS_Linkage* AVS_linkage;

#ifdef _WIN32
#define MEDIAN_EXPORT __declspec(dllexport)
#else
#define MEDIAN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" MEDIAN_EXPORT const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, AVS_Linkage * AVS_linkage_arg)
{
  AVS_linkage = AVS_linkage_arg;

  env->AddFunction("Median", "c+[CHROMA]b[SYNC]i[SYNCX]i[SYNCY]i[IGNORE_T]i[IGNORE_B]i[IGNORE_L]i[IGNORE_R]i[SAMPLES]i[DEBUG]b[THREADS]i[OPT]i[OPENCL]b[DEVICE_TYPE]s[DEVICE_ID]i", Create_Median, 0);
  env->AddFunction("TemporalMedian", "c[RADIUS]i[CHROMA]b[DEBUG]b[THREADS]i[OPT]i[OPENCL]b[DEVICE_TYPE]s[DEVICE_ID]i", Create_TemporalMedian, 0);
  env->AddFunction("MedianBlend", "c+[LOW]i[HIGH]i[CHROMA]b[SYNC]i[SYNCX]i[SYNCY]i[IGNORE_T]i[IGNORE_B]i[IGNORE_L]i[IGNORE_R]i[SAMPLES]i[DEBUG]b[THREADS]i[OPT]i[OPENCL]b[DEVICE_TYPE]s[DEVICE_ID]i", Create_MedianBlend, 0);

  return "Median of clips filter";
}
