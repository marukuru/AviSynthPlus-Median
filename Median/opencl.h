#ifndef MEDIAN_OPENCL_H
#define MEDIAN_OPENCL_H
#include "kernels.h"
#include "opencl_device.h"
#include <memory>

namespace median {
class OpenCLProcessor {
public:
  OpenCLProcessor(int component_size, int depth, int low, int high,
                  OpenCLDeviceType device_type = OpenCLDeviceType::Auto, int device_id = 0);
  ~OpenCLProcessor();
  // Returns false for float averaging or devices without faithful float support.
  bool process(const PlaneJob& job);
private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
}
#endif
