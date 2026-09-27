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
Median(clip1, clip2, clip3, ..., bool chroma=true, int sync=0, int syncx=0, int syncy=0, int ignore_t=0, int ignore_b=0, int ignore_l=0, int ignore_r=0, int samples=4096, bool debug=false, int threads=1)
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
- **threads**: Number of threads to use for parallel processing (default 1).

Spatial sync can be used with `sync=0` to align corresponding frames, or combined with temporal sync. The selected offsets are applied before calculating the median. Use non-negative values for the search radii and border exclusions. Border exclusions are measured in input-frame pixels and affect only sync comparisons; they do not crop the output. They have no effect when `sync`, `syncx`, and `syncy` are all 0.

For example, search up to one frame and two pixels in either direction while ignoring the bottom 16 rows during matching:

```avisynth
Median(clip1, clip2, clip3, sync=1, syncx=2, syncy=2, ignore_b=16)
```

### `TemporalMedian`
Applies a temporal median filter on a single clip.
```avisynth
TemporalMedian(clip, int radius=1, bool chroma=true, bool debug=false, int threads=1)
```
- **clip**: The input clip.
- **radius**: Temporal radius (1 to 12, default 1).
- **chroma**: Process chroma.
- **debug**: Enable debug output.
- **threads**: Number of threads to use.

### `MedianBlend`
A more configurable median function that allows dropping the highest and lowest extremes and blending the rest.
```avisynth
MedianBlend(clip1, clip2, clip3, ..., int low=1, int high=1, bool chroma=true, int sync=0, int syncx=0, int syncy=0, int ignore_t=0, int ignore_b=0, int ignore_l=0, int ignore_r=0, int samples=4096, bool debug=false, int threads=1)
```

- **clip1, clip2, ...**: Requires between 3 and 25 clips.
- **low**: Number of lowest pixel values to discard.
- **high**: Number of highest pixel values to discard.
- *(Remaining parameters, including spatial sync and border exclusions, are the same as `Median`.)*

## Change log

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

