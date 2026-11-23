# Downloads ONNX Runtime (MIT, Microsoft) for Strumline's chord converter and lays it out where plugin/CMakeLists.txt looks for it.
#
#   cmake -P tools/fetch_onnxruntime.cmake <folder> [version]
#
# It takes the official NuGet package Microsoft.ML.OnnxRuntime (about 184 MB: it holds every system), keeps the headers, the licence and the shared
# libraries of Windows, Linux and macOS, and deletes the rest. Result:
#   <folder>/include/*.h
#   <folder>/runtimes/<win-x64|linux-x64|linux-arm64|osx-x64|osx-arm64>/native/*   (whichever the package has)
#   <folder>/LICENSE, ThirdPartyNotices.txt
# Then configure Strumline with -DSTRUMLINE_ORT_DIR=<folder> (or put the folder where the default points: ../../SAS-datasets/onnxruntime from the repository).
cmake_minimum_required(VERSION 3.25)

# Arguments after "-P script": CMAKE_ARGV3 is the folder, CMAKE_ARGV4 the version.
if(NOT CMAKE_ARGV3)
  message(FATAL_ERROR "usage: cmake -P tools/fetch_onnxruntime.cmake <folder> [version]")
endif()
get_filename_component(_dest "${CMAKE_ARGV3}" ABSOLUTE)
set(_version "1.32.0")
if(CMAKE_ARGV4)
  set(_version "${CMAKE_ARGV4}")
endif()

set(_tmp "${_dest}/_download")
file(MAKE_DIRECTORY "${_tmp}")
set(_pkg "${_tmp}/ort.nupkg")
set(_url "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${_version}/microsoft.ml.onnxruntime.${_version}.nupkg")
if(EXISTS "${_pkg}")
  message(STATUS "Using the package already downloaded: ${_pkg}")
else()
message(STATUS "Downloading ${_url} (about 184 MB)")
file(DOWNLOAD "${_url}" "${_pkg}" STATUS _status SHOW_PROGRESS)
list(GET _status 0 _code)
if(NOT _code EQUAL 0)
  message(FATAL_ERROR "download failed: ${_status}")
endif()
endif()

# The whole package is extracted (a pattern that matches nothing is an error, and which systems a version ships changes: 1.32 has no osx-x64).
message(STATUS "Extracting")
file(ARCHIVE_EXTRACT INPUT "${_pkg}" DESTINATION "${_tmp}/x")

file(MAKE_DIRECTORY "${_dest}/include" "${_dest}/runtimes")
file(GLOB _headers "${_tmp}/x/build/native/include/*")
file(COPY ${_headers} DESTINATION "${_dest}/include")
foreach(_rid win-x64 linux-x64 linux-arm64 osx-x64 osx-arm64)
  if(EXISTS "${_tmp}/x/runtimes/${_rid}/native")
    file(GLOB _native "${_tmp}/x/runtimes/${_rid}/native/*")
    # The import libraries (.lib) and the static ones are not needed: the library is opened by hand.
    list(FILTER _native EXCLUDE REGEX "\\.(lib|a)$")
    file(COPY ${_native} DESTINATION "${_dest}/runtimes/${_rid}/native")
    message(STATUS "  ${_rid}: kept")
  else()
    message(STATUS "  ${_rid}: not in this package")
  endif()
endforeach()
foreach(_f LICENSE ThirdPartyNotices.txt)
  if(EXISTS "${_tmp}/x/${_f}")
    file(COPY "${_tmp}/x/${_f}" DESTINATION "${_dest}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_tmp}")
message(STATUS "ONNX Runtime ${_version} is in ${_dest}")
