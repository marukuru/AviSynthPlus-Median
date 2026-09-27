#include "avisynth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

const AVS_Linkage* AVS_linkage = nullptr;

static std::vector<int> planes(const VideoInfo& vi)
{
  if (!vi.IsPlanar()) return {0};
  std::vector<int> result = vi.IsRGB() ? std::vector<int>{PLANAR_G, PLANAR_B, PLANAR_R}
                                     : std::vector<int>{PLANAR_Y, PLANAR_U, PLANAR_V};
  result.resize(vi.NumComponents(), PLANAR_A);
  return result;
}

class Pattern : public GenericVideoFilter {
  int seed;
  bool shifted;
public:
  Pattern(PClip clip, int seed, bool shifted) : GenericVideoFilter(clip), seed(seed), shifted(shifted) {}
  int __stdcall SetCacheHints(int hint, int) override { return hint == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override {
    auto f = env->NewVideoFrame(vi);
    for (int plane : planes(vi)) {
      for (int y = 0; y < f->GetHeight(plane); ++y) {
        auto row = f->GetWritePtr(plane) + y * f->GetPitch(plane);
        for (int x = 0; x < f->GetRowSize(plane) / vi.ComponentSize(); ++x) {
          int sx = x + (shifted ? (n + seed) % 5 - 2 : 0);
          int sy = y + (shifted ? (n + seed) % 3 - 1 : 0);
          uint32_t value = static_cast<uint32_t>(sx * 7919 + sy * 104729 + n * 1543 + (shifted ? 0 : seed * 3331) + plane * 97);
          value ^= value << 13; value ^= value >> 17; value ^= value << 5;
          if (vi.ComponentSize() == 1) row[x] = static_cast<uint8_t>(value);
          else if (vi.ComponentSize() == 2) reinterpret_cast<uint16_t*>(row)[x] = value & ((1U << vi.BitsPerComponent()) - 1);
          else reinterpret_cast<float*>(row)[x] = (static_cast<int>(value % 2048) - 1024) / 1024.0f;
        }
      }
    }
    return f;
  }
};
static AVSValue __cdecl make_pattern(AVSValue args, void*, IScriptEnvironment*)
{
  return new Pattern(args[0].AsClip(), args[1].AsInt(), args[2].AsBool(false));
}
static PClip eval(IScriptEnvironment* env, const std::string& script)
{
  return env->Invoke("Eval", script.c_str()).AsClip();
}
static double sample(PVideoFrame f, int p, int x, int y, int cs)
{
  const auto row = f->GetReadPtr(p) + y * f->GetPitch(p);
  return cs == 1 ? row[x] : cs == 2 ? reinterpret_cast<const uint16_t*>(row)[x] : reinterpret_cast<const float*>(row)[x];
}
static void verify(IScriptEnvironment* env, PClip result, const std::vector<PClip>& clips,
                   int low, int high, bool chroma, bool temporal)
{
  const auto& vi = result->GetVideoInfo();
  for (int n : {0, 1, 7, 15}) {
    std::vector<PVideoFrame> inputs;
    for (size_t i = 0; i < clips.size(); ++i)
      inputs.push_back(clips[i]->GetFrame(temporal ? std::clamp(n - low + static_cast<int>(i), 0, vi.num_frames - 1) : n, env));
    auto out = result->GetFrame(n, env);
    for (int p : planes(vi)) {
      const int cs = vi.ComponentSize();
      for (int y = 0; y < out->GetHeight(p); ++y) {
        for (int x = 0; x < out->GetRowSize(p) / cs; ++x) {
          std::vector<double> values;
          for (const auto& f : inputs) values.push_back(sample(f, p, x, y, cs));
          bool copy = !chroma && (vi.IsPlanar() ? (p == PLANAR_A || (!vi.IsRGB() && (p == PLANAR_U || p == PLANAR_V)))
                        : (vi.IsYUY2() ? x % 2 == 1 : vi.NumComponents() == 4 && x % 4 == 3));
          double expected = values[0];
          if (!copy) {
            if (low || high) std::sort(values.begin(), values.end());
            expected = 0;
            for (size_t i = low; i < values.size() - high; ++i) expected += values[i];
            expected /= values.size() - low - high;
            if (cs != 4) expected = std::floor(expected);
          }
          if (std::abs(sample(out, p, x, y, cs) - expected) > 0.00000012)
            throw std::runtime_error("Pixel mismatch at frame " + std::to_string(n) + ", plane " + std::to_string(p));
        }
      }
    }
  }
}

static void expect_error(IScriptEnvironment* env, const std::string& script, const char* message)
{
  try { eval(env, script); }
  catch (const AvisynthError& e) {
    if (!std::strstr(e.msg, message)) throw;
    return;
  }
  throw std::runtime_error("Expected parameter error for: " + script);
}

static void run(IScriptEnvironment* env, const char* plugin, bool gpu)
{
  std::cout << "Loading " << plugin << std::endl;
  PNeoEnv neo(env);
  if (!neo) throw std::runtime_error("Extended AviSynth+ environment unavailable");
  static_cast<IScriptEnvironment2*>(neo)->ClearAutoloadDirs();
  env->Invoke("LoadPlugin", AVSValue(plugin));
  std::cout << "Loaded" << std::endl;
  env->AddFunction("TestPattern", "c[seed]i[shifted]b", make_pattern, nullptr);
  const std::vector<std::string> formats = {"Y8", "Y16", "Y32", "YUV420P8", "YUV420P10", "YUV444P16", "YUV444PS", "RGBP16", "RGBAPS", "YUVA444P8", "YUY2", "RGB24", "RGB32", "RGB48", "RGB64"};
  for (const auto& fmt : formats) {
    std::cout << "Checking " << fmt << std::endl;
    std::vector<PClip> clips;
    for (int i = 0; i < 5; ++i) {
      auto clip = eval(env, "TestPattern(BlankClip(width=68,height=96,length=16,pixel_type=\"" + fmt + "\"),seed=" + std::to_string(i) + ")");
      clips.push_back(clip);
      env->SetGlobalVar(env->SaveString(("c" + std::to_string(i)).c_str()), clip);
    }
    for (bool chroma : {true, false}) {
      for (int threads : (gpu ? std::vector<int>{1, 0} : std::vector<int>{1, 0, 999})) {
        std::string options = ",opencl=" + std::string(gpu ? "true" : "false") + ",opt=" + std::string(threads == 1 ? "1" : "0") + ",chroma=" + std::string(chroma ? "true" : "false") + ",threads=" + std::to_string(threads);
        std::string suffix = threads == 0 ? ".Prefetch(4)" : "";
        verify(env, eval(env, "Median(c0,c1,c2,c3,c4" + options + ")" + suffix), clips, 2, 2, chroma, false);
        verify(env, eval(env, "MedianBlend(c0,c1,c2,c3,c4,low=1,high=2" + options + ")" + suffix), clips, 1, 2, chroma, false);
        verify(env, eval(env, "TemporalMedian(c0,radius=2" + options + ")" + suffix), std::vector<PClip>(5, clips[0]), 2, 2, chroma, true);
      }
    }
  }
  const std::vector<std::string> functions = {"Median(c0,c1,c2", "MedianBlend(c0,c1,c2", "TemporalMedian(c0"};
  for (const auto& function : functions) {
    expect_error(env, function + ",device_type=\"invalid\")", "device_type");
    expect_error(env, function + ",device_id=-1)", "device_id");
    // Device controls are accepted and validated even in builds without OpenCL;
    // valid selections are ignored until OpenCL is explicitly enabled.
    for (const char* type : {"auto", "cpu", "gpu", "accelerator"})
      eval(env, function + ",opencl=false,device_type=\"" + type + "\",device_id=99)")->GetFrame(0, env);
    if (gpu) expect_error(env, function + ",opencl=true,device_id=2147483647)", "device_id=2147483647");
  }
  if (gpu) {
    const std::vector<PClip> clips = {env->GetVar("c0").AsClip(), env->GetVar("c1").AsClip(), env->GetVar("c2").AsClip()};
    for (const char* type : {"cpu", "gpu", "accelerator"}) {
      try {
        const std::string options = std::string(",opencl=true,device_type=\"") + type + "\",device_id=0)";
        verify(env, eval(env, "Median(c0,c1,c2" + options), clips, 1, 1, true, false);
        verify(env, eval(env, "MedianBlend(c0,c1,c2,low=0,high=1" + options), clips, 0, 1, true, false);
        verify(env, eval(env, "TemporalMedian(c0" + options), std::vector<PClip>(3, clips[0]), 1, 1, true, true);
        std::cout << "Verified explicit OpenCL device_type=" << type << std::endl;
      } catch (const AvisynthError& e) {
        if (!(std::strstr(e.msg, "device_type=") && (std::strstr(e.msg, "out of range") || std::strstr(e.msg, "unavailable")))) throw;
        std::cout << "No usable OpenCL device_type=" << type << " on this host" << std::endl;
      }
    }
  }
  // Exercise frame-varying alignment concurrently, including chroma subsampling.
  for (int i = 0; i < 3; ++i)
    env->SetGlobalVar(env->SaveString(("s" + std::to_string(i)).c_str()), eval(env, "TestPattern(BlankClip(width=68,height=96,length=16,pixel_type=\"YUV420P10\"),seed=" + std::to_string(i) + ",shifted=true)"));
  const std::string script = "Median(s0,s1,s2,sync=1,syncx=2,syncy=2,samples=0,ignore_b=4,threads=";
  auto serial = eval(env, script + "1)");
  auto parallel = eval(env, script + (gpu ? "0,opencl=true).Prefetch(4)" : "0).Prefetch(4)"));
  for (int n = 0; n < 16; ++n) {
    auto a = serial->GetFrame(n, env), b = parallel->GetFrame(n, env);
    for (int p : planes(serial->GetVideoInfo()))
      for (int y = 0; y < a->GetHeight(p); ++y)
        if (std::memcmp(a->GetReadPtr(p) + y * a->GetPitch(p), b->GetReadPtr(p) + y * b->GetPitch(p), a->GetRowSize(p)))
          throw std::runtime_error("Prefetch alignment differs from serial output");
  }
  try {
    eval(env, "Median(c0,c1,c2,threads=-1)");
    throw std::runtime_error("Negative threads accepted");
  } catch (const AvisynthError&) {}
#if !MEDIAN_TEST_HAS_OPENCL
  try {
    eval(env, "Median(c0,c1,c2,opencl=true)");
    throw std::runtime_error("OpenCL request unexpectedly accepted by CPU-only build");
  } catch (const AvisynthError& e) {
    if (!std::strstr(e.msg, "not compiled")) throw;
  }
#endif
}
int main(int argc, char** argv)
{
  if (argc != 2 && argc != 3) return 2;
  auto env = CreateScriptEnvironment(8);
  if (!env) return 2;
  AVS_linkage = env->GetAVSLinkage();

  int result = 0;
  try { run(env, argv[1], argc == 3); std::cout << "Native threading and pixel reference checks passed\n"; }
  catch (const IScriptEnvironment::NotFound&) { std::cerr << "AviSynth function not found\n"; result = 1; }
  catch (const AvisynthError& e) { std::cerr << e.msg << '\n'; result = argc == 3 && (std::strstr(e.msg, "No OpenCL platform") || std::strstr(e.msg, "No available OpenCL")) ? 77 : 1; }
  catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
  env->DeleteScriptEnvironment();
  return result;
}
