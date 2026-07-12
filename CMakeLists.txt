cmake_minimum_required(VERSION 3.24)
project(cppreact VERSION 0.1.0 LANGUAGES CXX)

add_library(cppreact INTERFACE)
add_library(cppreact::cppreact ALIAS cppreact)

target_include_directories(cppreact INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(cppreact INTERFACE cxx_std_20)

option(CPPREACT_TESTS "build the cppreact tests" ${PROJECT_IS_TOP_LEVEL})

if(CPPREACT_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(catch2)
  add_subdirectory(tests)
endif()
