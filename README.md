> [!NOTE]
> **AI-assisted development: CODEX GPT-6 Astra Extra High**

# Median
An Avisynth Median Filter by ajk

## Links

Forum thread: http://forum.doom9.org/showthread.php?t=170216

Forum thread: http://forum.videohelp.com/threads/362361-Median%28%29-plugin-for-AviSynth

https://forum.doom9.org/showthread.php?p=1864302#post1864302

https://forum.doom9.org/showpost.php?p=1864406&postcount=55

Vapoursynth port https://github.com/dubhater/vapoursynth-median

## Usage

The plugin provides three functions: `Median`, `TemporalMedian`, and `MedianBlend`.

### `Median`
Calculates a pixel-by-pixel median across multiple input clips.
```avisynth
Median(clip1, clip2, clip3, ..., bool chroma=true, int sync=0, int syncx=0, int syncy=0, int ignore_t=0, int ignore_b=0, int ignore_l=0, int ignore_r=0, int samples=4096, bool debug=false, int threads=1, int opt=0, bool opencl=false)
```

- **clip1, clip2, ...**: Requires an odd number of clips between 3 and 25. All clips must have the same format and dimensions.
- **chroma**: Set to `false` to disable chroma processing (or alpha channel processing for RGB32).
- **sync**: Radius in frames for temporal sync against the first clip (default 0).
- **syncx**: Horizontal search radius in pixels for spatial sync against the first clip. Searches offsets from `-syncx` to `+syncx` (default 0, disabled).
- **syncy**: Vertical search radius in pixels for spatial sync against the first clip. Searches offsets from `-syncy` to `+syncy` (default 0, disabled).
- **ignore_t**: Number of pixels to exclude from the top edge when comparing frames for sync (default 0).
- **ignore_b**: Number of pixels to exclude from the bottom edge when comparing frames for sync (default 0).
- **ignore_l**: Number of pixels to exclude from the left edge when comparing frames for sync (default 0).
- **ignore_r**: Number of pixels to exclude from the right edge when comparing frames for sync (default 0).
- **samples**: Number of points to sample for sync calculations.
- **debug**: Set to `true` to print debug information on the output frames.
- **opencl**: Opt in to GPU processing (default `false`); requires an OpenCL-enabled build. See below.
- **opt**: CPU kernel selection (default `0`, automatic). See the table below.
- **threads**: Maximum workers for within-frame processing using AviSynth+'s native thread pool. `0` uses the pool size; `1` disables within-frame parallelism (default). Negative values are rejected.

Spatial sync can be used with `sync=0` to align corresponding frames, or combined with temporal sync. The selected offsets are applied before calculating the median. Use non-negative values for the search radii and border exclusions. Border exclusions are measured in input-frame pixels and affect only sync comparisons; they do not crop the output. They have no effect when `sync`, `syncx`, and `syncy` are all 0.

For example, search up to one frame and two pixels in either direction while ignoring the bottom 16 rows during matching:

```avisynth
Median(clip1, clip2, clip3, sync=1, syncx=2, syncy=2, ignore_b=16)
```

### `TemporalMedian`
Applies a temporal median filter on a single clip.
```avisynth
TemporalMedian(clip, int radius=1, bool chroma=true, bool debug=false, int threads=1, int opt=0, bool opencl=false)
```
- **clip**: The input clip.
- **radius**: Temporal radius (1 to 12, default 1).
- **chroma**: Process chroma.
- **debug**: Enable debug output.
- **opencl**: Same optional GPU backend as `Median` (default `false`).
- **opt**: Same CPU kernel selection as `Median` (default `0`).
- **threads**: Same native thread-pool control as `Median` (default 1).

### `MedianBlend`
A more configurable median function that allows dropping the highest and lowest extremes and blending the rest.
```avisynth
MedianBlend(clip1, clip2, clip3, ..., int low=1, int high=1, bool chroma=true, int sync=0, int syncx=0, int syncy=0, int ignore_t=0, int ignore_b=0, int ignore_l=0, int ignore_r=0, int samples=4096, bool debug=false, int threads=1, int opt=0, bool opencl=false)
```

- **clip1, clip2, ...**: Requires between 3 and 25 clips.
- **low**: Number of lowest pixel values to discard.
- **high**: Number of highest pixel values to discard.
- *(Remaining parameters, including spatial sync and border exclusions, are the same as `Median`.)*

## CPU acceleration

The plugin checks AviSynth+'s CPU/OS feature flags at runtime. The default is portable across machines; only the selected kernels use advanced instructions. Explicit unavailable modes produce an error.

| `opt` | Kernels | Required CPU/OS features |
| --- | --- | --- |
| 0 | Automatic: 8, 6, 7, 5, 4, 3, 2, then 1 | None beyond the build target |
| 1 | C++ reference | None beyond the build target |
| 2 | SSE2 | SSE2 |
| 3 | SSE4.1 | SSE2, SSE4.1 |
| 4 | AVX | SSE2, SSE4.1, AVX with OS support |
| 5 | AVX2 | SSE2, SSE4.1, AVX with OS support, AVX2 |
| 6 | FMA3 | AVX2 requirements plus FMA3 |
| 7 | FMA4 | AVX2 requirements plus FMA4 |
| 8 | AVX512 | AVX2 requirements, FMA3, AVX512F/DQ/BW/VL with OS support |

SIMD accelerates median selection for 8-bit, 10–16-bit, and float samples, including shifted rows and packed formats. SSE4.1 improves 16-bit min/max; AVX widens float processing; AVX2 widens integer processing; AVX512 processes 64 bytes, 32 words, or 16 floats per vector. Each mode uses the appropriate narrower kernel for formats that do not benefit from its extra instructions. FMA3/FMA4 modes also vectorize float `MedianBlend` averaging with double-precision accumulation and FMA-refined reciprocal division; last-bit float rounding can differ from `opt=1`. Integer averaging uses the C++ path. Unaligned rows and incomplete vector tails are supported.

FMA3/FMA4 modes reuse AVX2/AVX for median selection, where fused arithmetic is not needed. MMX2, SSSE3, SSE4.2, and BMI2 do not add useful operations to these comparison networks and have no separate modes. `ENABLE_INTEL_SIMD=OFF` builds the portable C++ path only (`opt=0` or `1`).

## Multithreading

All three filters report `MT_NICE_FILTER` and keep frame-specific state local, so AviSynth+ can process multiple frames concurrently with [Prefetch](https://avisynthplus.readthedocs.io/en/3.7/avisynthdoc/syntax/syntax_internal_functions_multithreading_new.html). For example:

```avisynth
Median(clip1, clip2, clip3, threads=1)
Prefetch(4)
```

Start with `threads=1` when using frame-level prefetch. For expensive individual frames, `threads=0` or a positive worker limit also enables row parallelism in the native AviSynth+ pool (3.6 or newer). Work is capped to the pool size and the number of useful row blocks. Older hosts without the extended environment use serial row processing.

## Optional OpenCL

OpenCL is disabled by default at build time and in scripts. To build it with CMake, install OpenCL development headers and the ICD loader (on Ubuntu/Debian: `ocl-icd-opencl-dev`) and a working GPU driver, then run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMEDIAN_ENABLE_OPENCL=ON
cmake --build build
```

For the Visual Studio solution, set MSBuild properties `MedianOpenCL=true`, `OpenCLIncludeDir` to the SDK include directory, and `OpenCLLibraryDir` to the directory containing the matching architecture's `OpenCL.lib`.

Enable it per filter:

```avisynth
Median(clip1, clip2, clip3, opencl=true)
Prefetch(4)
```

The backend selects the first available OpenCL 1.2 GPU with a compiler. It supports integer median/blending and float median selection, including packed formats, spatial shifts, and chroma bypass. Float averaging stays on the CPU to retain double-precision accumulation; float processing also falls back to the CPU if the device lacks denormal or infinity/NaN support. Sync searches remain on the CPU. `opt` and `threads` control CPU work; GPU dispatch is managed by OpenCL.

Each filter reuses its device buffers and serializes GPU submissions, making concurrent `Prefetch` requests safe. Frame uploads and downloads add overhead, so benchmark your workload against the default CPU SIMD path. Missing build support, unavailable GPUs, allocation failures, and driver/build errors produce explicit errors when `opencl=true`.

## Change log

20260927 Unreleased (marukuru)

  - Add spatial alignment with `syncx` and `syncy`, and sync comparison border exclusions with `ignore_t`, `ignore_b`, `ignore_l`, and `ignore_r`.
  - Extend processing to high-bit-depth integer and float planes, planar RGB/RGBA and YUVA, and packed RGB48. Preserve fractional and negative float values during blending.
  - Make all three filters safe for concurrent AviSynth+ `Prefetch` requests with `MT_NICE_FILTER` and frame-local alignment state. Use the native worker pool for row parallelism; `threads=0` selects the pool size automatically.
  - Add `opt=0–8` CPU selection with automatic dispatch, a C++ reference path, SSE2, SSE4.1, AVX, AVX2, FMA3, FMA4, and AVX512 kernels. FMA modes accelerate float blending.
  - Add optional OpenCL 1.2 GPU processing through `opencl=true` and `MEDIAN_ENABLE_OPENCL=ON`, disabled by default. Support integer median/blending and float median selection, with CPU fallback for float averaging and unsupported float device capabilities.
  - Speed up row processing and sync sampling, and reuse candidate frames during alignment searches.
  - Correct handling of per-clip pitches, subsampled chroma offsets, and temporal frame boundaries. Fix high-bit-depth debug text and Linux plugin linkage symbol collisions.
  - Update CMake and Visual Studio builds, document the new parameters, and add CPU/OpenCL reference, boundary, and concurrent-processing tests.

20220301 v0.7 (pinterf)
  - move to github: https://github.com/pinterf/Median
  - add README.md, build
  - add Window version resource to DLL
  - Update Avisynth headers
  - pass frame properties
  - move to VS2019 (v142 toolset)
  - add CMake build environment
  - Linux/GCC friendly source
  - DLL/so name is changed to Median/libmedian (from simple Median - possible name collisions)

20190201 v0.6 (TomArrow)
  - https://forum.doom9.org/showthread.php?p=1864406#post1864406
  - Support RGB64 (but not the fast processing mode that's supported in the 8-bit color spaces)
  - Implement the new AviSynth+ API (V6)
  - Have both 32 bit and 64 bit platforms set up for VS 2017 (not sure if backwards compatible for older VS versions, but maybe?)

201511xx 0.6 (ajk)
  - Added sync functionality

20140215 0.5 (ajk)
  - the plugin will accept between 3 and 25 clips
  - chroma processing can be turned off with "chroma=false", or in the case of RGB32 this will turn off processing for the alpha channel
  - there is also a more configurable MedianBlend() function, see examples in the readme or posts further below
  - Added TemporalMedian functionality 
    https://forum.doom9.org/showpost.php?p=1667483&postcount=1

20140214 0.4 (ajk)
  - Added MedianBlend functionality

20140213 0.3 (ajk)
  - Added support for RGB and planar formats

20140212 0.1 (ajk)
  - Initial release. YUY2 support only

## Build Instructrions

### Windows MSVC

* build from IDE

## Windows GCC
(mingw installed by msys2)
From the 'build' folder under project root:

    del ..\CMakeCache.txt
    cmake .. -G "MinGW Makefiles"
    cmake --build . --config Release  

## Linux build instructions

### Prerequisites

To compile on Ubuntu or Debian-based systems, install the following packages:
```bash
sudo apt update
sudo apt install build-essential cmake git
```

### Building
* Clone repo

        git clone https://github.com/pinterf/Median
        cd Median
        cmake -B build -S .
        cmake --build build

  Useful hints:        
  build after clean:

      cmake --build build --clean-first

  delete CMake cache

      rm build/CMakeCache.txt

* Find binaries at

        build/Median/Median.so

* Install binaries

        cd build
        sudo make install


### Tests

With the AviSynth+ runtime library installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMEDIAN_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `build/tests/median_benchmark` for a 1080p kernel-only comparison of `opt=1` and `opt=0` (excludes decoding, sync search, frame scheduling, and I/O).

Tests compare integer and float output with independent pixel references and cover packed/planar formats, alpha, chroma bypass, different strides, shifted edges, temporal boundaries, native worker jobs, and concurrent `Prefetch` requests.
