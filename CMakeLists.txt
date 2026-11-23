cmake_minimum_required(VERSION 3.25)
# The version is written here and only here (the plug-in, the installer and package.ps1 read it). See docs/RELEASING.md.
set(STRUMLINE_VERSION 0.1.0)
set(STRUMLINE_VERSION_SUFFIX "-beta")
project(Strumline VERSION ${STRUMLINE_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "" FORCE)
# JUCE: the same version as Schematic Amp Sim. When that project is next to this folder and has already fetched it, that copy is used
# (no second download).
set(_sas_juce "${CMAKE_SOURCE_DIR}/../SchematicAmpSim/third_party/juce-src")
if(EXISTS "${_sas_juce}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_JUCE "${_sas_juce}" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.16-security
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(probe)

add_subdirectory(plugin)
