# Visual Studio 2019 is supported from CMake 3.14.1
# Tested generators:
# "MinGW Makefiles": MSYS2/Mingw32 GCC 8.3 build
# "Visual Studio 16 2019" optional platform generator Win32 and x64
# "Visual Studio 16 2019" + LLVM 8.0 (clang) optional platform generator Win32 and x64
CMAKE_MINIMUM_REQUIRED( VERSION 3.8.2 )

set(PluginName "Median")

if (NOT WIN32)
  string(TOLOWER "${PluginName}" PluginName)
endif()

set(ProjectName "${PluginName}")
project(${ProjectName} LANGUAGES CXX)

Include("Files.cmake")

add_library(${PluginName} SHARED ${Median_Sources})
# Keep the plugin linkage pointer and C++ internals separate from the host.
set_target_properties(${PluginName} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)

set(PLUGIN_VERSION "0.7")
set_target_properties(${PluginName} PROPERTIES "OUTPUT_NAME" "${PluginName}.${PLUGIN_VERSION}")
if (MINGW)
  set_target_properties(${PluginName} PROPERTIES PREFIX "")
  set_target_properties(${PluginName} PROPERTIES IMPORT_PREFIX "")
endif()

IF(ENABLE_INTEL_SIMD)
  #require sse2, some other plugins may need to set sse4.1 for quick msvc->gcc porting
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINTEL_INTRINSICS -msse2")
ENDIF()

# Automatically group source files according to directory structure
foreach(FILE ${Median_Sources}) 
  get_filename_component(PARENT_DIR "${FILE}" PATH)

  string(REGEX REPLACE "(\\./)" "" GROUP "${PARENT_DIR}")
  string(REPLACE "/" "\\" GROUP "${GROUP}")

  # group into "Source Files" and "Header Files"
  if ("${FILE}" MATCHES ".*\\.cpp")
    set(GROUP "Source Files\\${GROUP}")
  elseif("${FILE}" MATCHES ".*\\.h")
    set(GROUP "Header Files\\${GROUP}")
  elseif("${FILE}" MATCHES ".*\\.asm")
    set(GROUP "Assembler Files\\${GROUP}")
  endif()

  source_group("${GROUP}" FILES "${FILE}")
endforeach()

include(Cpu.cmake)
median_configure_cpu(${PluginName})

# Specify include directories
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#dedicated include dir for avisynth.h
#target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Windows DLL dependencies 
if (MSVC OR MINGW)
  target_link_libraries(${ProjectName} "uuid" "winmm" "vfw32" "msacm32" "gdi32" "user32" "advapi32" "ole32" "imagehlp")
else()
  #non Windows
  target_link_libraries(${ProjectName} "dl")
  # "pthread"  "dl"
endif()

include(GNUInstallDirs)

INSTALL(TARGETS ${ProjectName}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/avisynth")
