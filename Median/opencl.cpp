#include "opencl.h"
#include <stdexcept>
#include <string>

#ifdef MEDIAN_ENABLE_OPENCL
#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 120
#endif
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include <algorithm>
#include <cstdio>
#include <limits>
#include <mutex>
#include <vector>

namespace median {
namespace {
void check(cl_int status, const char* operation)
{
  if (status != CL_SUCCESS)
    throw std::runtime_error(std::string(operation) + " failed (OpenCL error " + std::to_string(status) + ")");
}
cl_device_id find_device(OpenCLDeviceType type, int index)
{
  cl_uint count = 0;
  const cl_int status = clGetPlatformIDs(0, nullptr, &count);
  if (status == -1001 || (status == CL_SUCCESS && count == 0))
    throw std::runtime_error("No OpenCL platform is available");
  check(status, "Platform enumeration");
  std::vector<cl_platform_id> platforms(count);
  check(clGetPlatformIDs(count, platforms.data(), nullptr), "Platform enumeration");
  auto enumerate = [&](OpenCLDeviceType requested) {
    const cl_device_type flags = requested == OpenCLDeviceType::CPU ? CL_DEVICE_TYPE_CPU
      : requested == OpenCLDeviceType::GPU ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_ACCELERATOR;
    std::vector<cl_device_id> devices;
    for (auto platform : platforms) {
      cl_uint device_count = 0;
      cl_int result = clGetDeviceIDs(platform, flags, 0, nullptr, &device_count);
      if (result == CL_DEVICE_NOT_FOUND) continue;
      check(result, "Device enumeration");
      if (!device_count) continue;
      const size_t offset = devices.size();
      devices.resize(offset + device_count);
      check(clGetDeviceIDs(platform, flags, device_count, devices.data() + offset, nullptr), "Device enumeration");
    }
    return devices;
  };
  auto usable = [](cl_device_id device) {
    cl_bool available = CL_FALSE, compiler = CL_FALSE;
    check(clGetDeviceInfo(device, CL_DEVICE_AVAILABLE, sizeof(available), &available, nullptr), "Device availability query");
    check(clGetDeviceInfo(device, CL_DEVICE_COMPILER_AVAILABLE, sizeof(compiler), &compiler, nullptr), "Device compiler query");
    if (!available || !compiler) return false;
    char version[128] = {};
    const cl_int result = clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, sizeof(version), version, nullptr);
    if (result == CL_INVALID_VALUE) return false; // OpenCL 1.0 does not expose this property.
    check(result, "Device language version query");
    int major = 0, minor = 0;
    std::sscanf(version, "OpenCL C %d.%d", &major, &minor);
    return major > 1 || (major == 1 && minor >= 2);
  };
  return select_opencl_device<cl_device_id>(type, index, enumerate, usable);
}
const char* source = R"CLC(
inline int less_value(TYPE a, TYPE b) {
#if FLOAT_INPUT
  return isnan(b) ? !isnan(a) : a < b;
#else
  return a < b;
#endif
}
__kernel void median_plane(__global const TYPE* input, __global TYPE* output,
                           __constant const int* offsets, int width, int height,
                           int components, int copy_every) {
  const int x = get_global_id(0), y = get_global_id(1);
  if (x >= width || y >= height) return;
  const size_t pixel = (size_t)y * width + x;
  if (copy_every && (x + 1) % copy_every == 0) { output[pixel] = input[pixel]; return; }
  TYPE v[DEPTH];
  for (int i = 0; i < DEPTH; ++i) {
    const int sx = clamp(x / components - offsets[i], 0, width / components - 1) * components + x % components;
    const int sy = clamp(y - offsets[DEPTH + i], 0, height - 1);
    v[i] = input[((size_t)i * height + sy) * width + sx];
  }
#if LOW || HIGH
  for (int i = 1; i < DEPTH; ++i) {
    TYPE value = v[i];
    int j = i;
    while (j > 0 && less_value(value, v[j - 1])) { v[j] = v[j - 1]; --j; }
    v[j] = value;
  }
#endif
#if DEPTH - LOW - HIGH == 1
  output[pixel] = v[LOW];
#elif !FLOAT_INPUT
  uint sum = 0;
  for (int i = LOW; i < DEPTH - HIGH; ++i) sum += v[i];
  output[pixel] = (TYPE)(sum / (DEPTH - LOW - HIGH));
#endif
}
)CLC";
}

struct OpenCLProcessor::Impl {
  cl_context context = nullptr;
  cl_command_queue queue = nullptr;
  cl_program program = nullptr;
  cl_kernel kernel = nullptr;
  cl_mem input = nullptr, output = nullptr, offsets = nullptr;
  size_t capacity = 0;
  cl_ulong max_allocation = 0;
  int component_size, depth, low, high;
  bool supported = true;
  std::mutex mutex;

  Impl(int cs, int n, int l, int h) : component_size(cs), depth(n), low(l), high(h) {}
  ~Impl() {
    if (queue) clFinish(queue);
    if (input) clReleaseMemObject(input);
    if (output) clReleaseMemObject(output);
    if (offsets) clReleaseMemObject(offsets);
    if (kernel) clReleaseKernel(kernel);
    if (program) clReleaseProgram(program);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
  }
  void initialize(OpenCLDeviceType type, int index) {
    const auto device = find_device(type, index);
    if (component_size == 4) {
      cl_device_fp_config fp = 0;
      check(clGetDeviceInfo(device, CL_DEVICE_SINGLE_FP_CONFIG, sizeof(fp), &fp, nullptr), "Float capability query");
      supported = depth - low - high == 1 && (fp & (CL_FP_DENORM | CL_FP_INF_NAN)) == (CL_FP_DENORM | CL_FP_INF_NAN);
      if (!supported) return;
    }
    check(clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(max_allocation), &max_allocation, nullptr), "Memory limit query");
    cl_int result = CL_SUCCESS;
    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &result);
    check(result, "Context creation");
    queue = clCreateCommandQueue(context, device, 0, &result);
    check(result, "Queue creation");
    program = clCreateProgramWithSource(context, 1, &source, nullptr, &result);
    check(result, "Program creation");
    std::string options = std::string("-cl-std=CL1.2 -D TYPE=") + (component_size == 1 ? "uchar" : component_size == 2 ? "ushort" : "float")
      + " -D FLOAT_INPUT=" + (component_size == 4 ? "1" : "0") + " -D DEPTH=" + std::to_string(depth)
      + " -D LOW=" + std::to_string(low) + " -D HIGH=" + std::to_string(high);
    result = clBuildProgram(program, 1, &device, options.c_str(), nullptr, nullptr);
    if (result != CL_SUCCESS) {
      size_t size = 0;
      clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &size);
      std::vector<char> log(size + 1, 0);
      clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, size, log.data(), nullptr);
      throw std::runtime_error("OpenCL kernel build failed: " + std::string(log.data()));
    }
    kernel = clCreateKernel(program, "median_plane", &result);
    check(result, "Kernel creation");
    offsets = clCreateBuffer(context, CL_MEM_READ_ONLY, 2 * depth * sizeof(cl_int), nullptr, &result);
    check(result, "Offset buffer allocation");
  }
  void reserve(size_t plane_bytes) {
    if (plane_bytes <= capacity) return;
    if (plane_bytes > std::numeric_limits<size_t>::max() / depth || plane_bytes > max_allocation / depth)
      throw std::runtime_error("Frame stack exceeds the OpenCL device's allocation limit");
    if (input) { clReleaseMemObject(input); input = nullptr; }
    if (output) { clReleaseMemObject(output); output = nullptr; }
    capacity = 0;
    cl_int result = CL_SUCCESS;
    input = clCreateBuffer(context, CL_MEM_READ_ONLY, plane_bytes * depth, nullptr, &result);
    check(result, "Input buffer allocation");
    output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, plane_bytes, nullptr, &result);
    check(result, "Output buffer allocation");
    capacity = plane_bytes;
  }
  bool process(const PlaneJob& job) {
    if (!supported) return false;
    if (job.depth != depth || job.low != low || job.high != high || job.component_size != component_size)
      throw std::runtime_error("OpenCL job does not match its kernel configuration");
    std::lock_guard<std::mutex> lock(mutex);
    const size_t row_bytes = static_cast<size_t>(job.width) * component_size;
    if (row_bytes > std::numeric_limits<size_t>::max() / job.height)
      throw std::runtime_error("OpenCL frame size overflow");
    const size_t plane_bytes = row_bytes * job.height;
    reserve(plane_bytes);
    cl_int shifts[2 * max_depth];
    for (int i = 0; i < depth; ++i) { shifts[i] = job.dx[i]; shifts[depth + i] = job.dy[i]; }
    const size_t zero[] = {0, 0, 0};
    const size_t region[] = {row_bytes, static_cast<size_t>(job.height), 1};
    try {
      for (int i = 0; i < depth; ++i) {
        const size_t origin[] = {0, 0, static_cast<size_t>(i)};
        check(clEnqueueWriteBufferRect(queue, input, CL_FALSE, origin, zero, region, row_bytes, plane_bytes,
          job.src_pitch[i], 0, job.src[i], 0, nullptr, nullptr), "Frame upload");
      }
      check(clEnqueueWriteBuffer(queue, offsets, CL_FALSE, 0, 2 * depth * sizeof(cl_int), shifts, 0, nullptr, nullptr), "Offset upload");
      check(clSetKernelArg(kernel, 0, sizeof(input), &input), "Input argument");
      check(clSetKernelArg(kernel, 1, sizeof(output), &output), "Output argument");
      check(clSetKernelArg(kernel, 2, sizeof(offsets), &offsets), "Offset argument");
      check(clSetKernelArg(kernel, 3, sizeof(job.width), &job.width), "Width argument");
      check(clSetKernelArg(kernel, 4, sizeof(job.height), &job.height), "Height argument");
      check(clSetKernelArg(kernel, 5, sizeof(job.components), &job.components), "Component argument");
      check(clSetKernelArg(kernel, 6, sizeof(job.copy_every), &job.copy_every), "Chroma argument");
      const size_t global[] = {static_cast<size_t>(job.width), static_cast<size_t>(job.height)};
      check(clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, global, nullptr, 0, nullptr, nullptr), "Kernel execution");
      check(clEnqueueReadBufferRect(queue, output, CL_TRUE, zero, zero, region, row_bytes, plane_bytes,
        job.dst_pitch, 0, job.dst, 0, nullptr, nullptr), "Frame download");
    } catch (...) {
      // Pending non-blocking transfers must finish before host frames or shifts die.
      clFinish(queue);
      throw;
    }
    return true;
  }
};
OpenCLProcessor::OpenCLProcessor(int cs, int n, int low, int high, OpenCLDeviceType type, int index)
  : impl(new Impl(cs, n, low, high)) { impl->initialize(type, index); }
OpenCLProcessor::~OpenCLProcessor() = default;
bool OpenCLProcessor::process(const PlaneJob& job) { return impl->process(job); }
}
#else
namespace median {
struct OpenCLProcessor::Impl {};
OpenCLProcessor::OpenCLProcessor(int, int, int, int, OpenCLDeviceType, int)
{
  throw std::runtime_error("OpenCL support was not compiled in; rebuild with MEDIAN_ENABLE_OPENCL=ON");
}
OpenCLProcessor::~OpenCLProcessor() = default;
bool OpenCLProcessor::process(const PlaneJob&) { return false; }
}
#endif
