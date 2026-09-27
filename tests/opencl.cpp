#include "opencl.h"
#include "kernels.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

template<typename T>
void verify(int depth, int low, int high, median::OpenCLDeviceType type = median::OpenCLDeviceType::Auto)
{
  median::OpenCLProcessor gpu(sizeof(T), depth, low, high, type, 0);
  for (int components : {1, 2, 3, 4}) {
    for (bool shifted : {false, true}) {
      median::PlaneJob job{};
      job.width = 43 * components;
      job.height = 17;
      job.end_y = job.height;
      job.component_size = sizeof(T);
      job.components = components;
      job.copy_every = components == 4 ? 4 : 0;
      job.depth = depth; job.low = low; job.high = high;
      job.dst_pitch = (job.width + 7) * sizeof(T);
      std::vector<T> output((job.width + 7) * job.height, T(42)), reference = output;
      std::vector<std::vector<T>> sources(depth);
      std::mt19937 rng(713);
      for (int i = 0; i < depth; ++i) {
        const int stride = job.width + 3 + i;
        sources[i].resize(stride * job.height);
        for (auto& value : sources[i]) value = sizeof(T) == 4 ? static_cast<T>((int(rng() % 1024) - 512) / 512.0) : static_cast<T>(rng());
        job.src[i] = reinterpret_cast<const uint8_t*>(sources[i].data());
        job.src_pitch[i] = stride * sizeof(T);
        job.dx[i] = shifted && i ? i % 5 - 2 : 0;
        job.dy[i] = shifted && i ? i % 3 - 1 : 0;
      }
      job.dst = reinterpret_cast<uint8_t*>(reference.data());
      median::process_plane(job);
      job.dst = reinterpret_cast<uint8_t*>(output.data());
      bool ran = gpu.process(job);
      if (sizeof(T) == 4 && depth - low - high != 1) {
        if (ran) throw std::runtime_error("Float averaging must fall back to CPU");
        continue;
      }
      if (!ran) {
        if (sizeof(T) != 4) throw std::runtime_error("Integer GPU processing unexpectedly skipped");
        std::cout << "Float device capabilities require CPU fallback\n";
        return;
      }
      for (size_t i = 0; i < output.size(); ++i)
        if (output[i] != reference[i]) throw std::runtime_error("OpenCL differs from CPU reference, including padding guards");
    }
  }
}
int main()
{
  try {
    for (int depth : {3, 5, 25}) {
      verify<uint8_t>(depth, depth / 2, depth / 2);
      verify<uint16_t>(depth, depth / 2, depth / 2);
      verify<float>(depth, depth / 2, depth / 2);
      verify<uint8_t>(depth, 0, 1);
      verify<uint16_t>(depth, 0, 1);
      verify<float>(depth, 0, 1);
    }
    for (auto type : {median::OpenCLDeviceType::CPU, median::OpenCLDeviceType::GPU, median::OpenCLDeviceType::Accelerator}) {
      try { verify<uint8_t>(3, 1, 1, type); }
      catch (const std::runtime_error& e) {
        const std::string error(e.what());
        if (error.find("device_type=") == std::string::npos || (error.find("out of range") == std::string::npos && error.find("unavailable") == std::string::npos)) throw;
      }
    }
    std::cout << "OpenCL/reference, shifts, pitches, guards and CPU fallback passed\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    const std::string error(e.what());
    return error.find("No OpenCL platform") != std::string::npos || error.find("No available OpenCL") != std::string::npos ? 77 : 1;
  }
}
