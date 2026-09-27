#include "opencl_device.h"
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <vector>

using Type = median::OpenCLDeviceType;
static void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
template<class F>
void expect_error(F run, const std::string& text)
{
  try { run(); }
  catch (const std::exception& e) {
    require(std::string(e.what()).find(text) != std::string::npos, "Unexpected device-selection error");
    return;
  }
  throw std::runtime_error("Invalid device selection accepted");
}
int main()
{
  try {
    require(median::parse_opencl_device_type("auto") == Type::Auto, "Auto parse failed");
    require(median::parse_opencl_device_type("cpu") == Type::CPU, "CPU parse failed");
    require(median::parse_opencl_device_type("gpu") == Type::GPU, "GPU parse failed");
    require(median::parse_opencl_device_type("accelerator") == Type::Accelerator, "Accelerator parse failed");
    for (const char* invalid : {"", "default", "GPU", "gpu "})
      expect_error([&] { median::parse_opencl_device_type(invalid); }, "device_type");

    std::array<std::vector<int>, 4> inventory;
    inventory[static_cast<int>(Type::Accelerator)] = {11, 12};
    inventory[static_cast<int>(Type::GPU)] = {21, 22};
    inventory[static_cast<int>(Type::CPU)] = {31, 32};
    std::set<int> unavailable;
    std::vector<Type> queried;
    auto choose = [&](Type type, int index) {
      queried.clear();
      return median::select_opencl_device<int>(type, index,
        [&](Type candidate) { queried.push_back(candidate); return inventory[static_cast<int>(candidate)]; },
        [&](int device) { return unavailable.count(device) == 0; });
    };
    require(choose(Type::Auto, 0) == 11, "Auto did not prefer accelerator");
    require(queried == std::vector<Type>{Type::Accelerator}, "Auto queried lower-priority types unnecessarily");
    require(choose(Type::Auto, 1) == 12, "Auto did not preserve the requested index");
    require(choose(Type::GPU, 1) == 22, "Explicit second GPU selection failed");
    require(queried == std::vector<Type>{Type::GPU}, "Explicit GPU selection used another type");
    require(choose(Type::CPU, 0) == 31, "Explicit CPU selection failed");
    unavailable.insert(11);
    require(choose(Type::Auto, 0) == 21, "Unavailable accelerator did not fall back to GPU");
    unavailable.insert(21);
    require(choose(Type::Auto, 0) == 31, "Unavailable GPU did not fall back to CPU");
    require(queried == std::vector<Type>{Type::Accelerator, Type::GPU, Type::CPU}, "Wrong auto fallback order");
    expect_error([&] { choose(Type::GPU, 0); }, "unavailable");
    // Unavailable devices retain their indices; selecting 1 still means device 22.
    require(choose(Type::GPU, 1) == 22, "Unavailable devices renumbered the index");
    inventory[static_cast<int>(Type::Accelerator)] = {11};
    require(choose(Type::Auto, 1) == 22, "Missing accelerator index did not fall back to GPU");
    inventory[static_cast<int>(Type::GPU)].clear();
    require(choose(Type::Auto, 1) == 32, "Missing GPU index did not fall back to CPU");
    expect_error([&] { choose(Type::GPU, 0); }, "out of range");
    expect_error([&] { choose(Type::CPU, 2); }, "device_id=2");
    expect_error([&] { choose(Type::Auto, std::numeric_limits<int>::max()); }, "device_id=2147483647");
    expect_error([&] { choose(Type::Auto, -1); }, "device_id must be at least 0");
    require(queried.empty(), "Negative device index reached enumeration");
    for (auto& devices : inventory) devices.clear();
    expect_error([&] { choose(Type::Auto, 0); }, "No available OpenCL");
    std::cout << "Device types, indices, availability errors and automatic fallback passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
