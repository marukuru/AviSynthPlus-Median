#ifndef MEDIAN_OPENCL_DEVICE_H
#define MEDIAN_OPENCL_DEVICE_H
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace median {
enum class OpenCLDeviceType { Auto, CPU, GPU, Accelerator };

inline OpenCLDeviceType parse_opencl_device_type(const std::string& type)
{
  if (type == "auto") return OpenCLDeviceType::Auto;
  if (type == "cpu") return OpenCLDeviceType::CPU;
  if (type == "gpu") return OpenCLDeviceType::GPU;
  if (type == "accelerator") return OpenCLDeviceType::Accelerator;
  throw std::invalid_argument("device_type must be \"auto\", \"cpu\", \"gpu\", or \"accelerator\".");
}

inline const char* opencl_device_type_name(OpenCLDeviceType type)
{
  switch (type) {
  case OpenCLDeviceType::CPU: return "cpu";
  case OpenCLDeviceType::GPU: return "gpu";
  case OpenCLDeviceType::Accelerator: return "accelerator";
  default: return "auto";
  }
}

// Keep selection independent of the OpenCL runtime so fallback order and device
// indexing can also be checked on systems without multiple physical devices.
template<typename Device, typename Enumerate, typename Usable>
Device select_opencl_device(OpenCLDeviceType type, int index, Enumerate enumerate, Usable usable)
{
  if (index < 0) throw std::invalid_argument("device_id must be at least 0.");
  for (auto candidate : {OpenCLDeviceType::Accelerator, OpenCLDeviceType::GPU, OpenCLDeviceType::CPU}) {
    if (type != OpenCLDeviceType::Auto && type != candidate) continue;
    const auto devices = enumerate(candidate);
    if (static_cast<std::size_t>(index) < devices.size() && usable(devices[index])) return devices[index];
    if (type != OpenCLDeviceType::Auto) {
      const std::string selection = std::string("device_type=\"") + opencl_device_type_name(type)
        + "\", device_id=" + std::to_string(index);
      if (static_cast<std::size_t>(index) >= devices.size())
        throw std::runtime_error("OpenCL " + selection + " is out of range (found " + std::to_string(devices.size()) + " devices).");
      throw std::runtime_error("OpenCL " + selection + " is unavailable or lacks an OpenCL 1.2 compiler.");
    }
  }
  throw std::runtime_error("No available OpenCL 1.2 device with a compiler for device_type=\"auto\", device_id="
    + std::to_string(index) + ". Tried accelerator, gpu, then cpu.");
}
}
#endif
