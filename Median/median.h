#ifndef MEDIAN_H
#define MEDIAN_H

#include <vector>
#include <stdint.h>
#include "kernels.h"
#include "opencl.h"

#define ERROR_PREFIX "Median: "

const unsigned int MAX_DEPTH = 25;
const unsigned int MAX_OPT = 9;

//////////////////////////////////////////////////////////////////////////////
// Class definition
//////////////////////////////////////////////////////////////////////////////

class Median : public GenericVideoFilter
{
public:
  Median(PClip _child, std::vector<PClip> _clips, unsigned int _low, unsigned int _high, bool _temporal, bool _processchroma, unsigned int _sync, unsigned int _syncx, unsigned int _syncy, unsigned int _samples, unsigned int _ignoret, unsigned int _ignoreb, unsigned int _ignorel, unsigned int _ignorer, bool _debug, unsigned int _threads, int opt, bool use_opencl, const char* device_type, int device_id, IScriptEnvironment* env);
  ~Median();

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  int __stdcall SetCacheHints(int cachehints, int frame_range) override;

public:
  bool has_at_least_v8; // passing frame property support

  std::vector<PClip> clips;
  unsigned int low;
  unsigned int high;
  bool temporal;
  bool processchroma;
  unsigned int sync;
  unsigned int syncx;
  unsigned int syncy;
  unsigned int samples;
  unsigned int ignoret;
  unsigned int ignoreb;
  unsigned int ignorel;
  unsigned int ignorer;
  bool debug;
  unsigned int threads;
  median::RowKernel row_kernel;
  std::unique_ptr<median::OpenCLProcessor> opencl;

  unsigned int depth;
  unsigned int blend;
  bool fastprocess;
  std::vector<VideoInfo> info;

  unsigned char (*fastmedian)(unsigned char*);

  double CompareFrames(int plane, PVideoFrame a, PVideoFrame b, unsigned int points, int dx, int dy);
  
  void ProcessFrame(PVideoFrame src[MAX_DEPTH], PVideoFrame& dst,
                    const int* match_x, const int* match_y, IScriptEnvironment* env);

  void debugf(const char* fmt, ...);

  
  void textf(PVideoFrame& dst, unsigned int& line, const char* fmt, ...);
};


#endif // MEDIAN_H
