cmake_minimum_required(VERSION 3.20)
project(FaultlineFramework VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
endif()

# The same static engine libraries can also be linked into the Python module.
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(FAULTLINE_GAME_V1)

add_library(faultline_sdk STATIC sdk/simulation.cpp)
add_library(faultline::sdk ALIAS faultline_sdk)
target_compile_features(faultline_sdk PUBLIC cxx_std_20)
target_include_directories(faultline_sdk PUBLIC sdk/include)
target_link_libraries(faultline_sdk PRIVATE faultline_persistence)
set_target_properties(faultline_sdk PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(faultline_sdk PRIVATE /W4 /permissive-)
else()
  target_compile_options(faultline_sdk PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

option(FAULTLINE_BUILD_PYTHON "Build the Python extension" OFF)
if(FAULTLINE_BUILD_PYTHON)
  set(PYBIND11_FINDPYTHON ON)
  find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)
  pybind11_add_module(_native NO_EXTRAS python/bindings.cpp)
  target_link_libraries(_native PRIVATE faultline::sdk)
  install(TARGETS _native
    LIBRARY DESTINATION faultline_sim COMPONENT python
    RUNTIME DESTINATION faultline_sim COMPONENT python)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND BUILD_TESTING)
  set(FAULTLINE_SDK_EMBEDDED_TEST ON)
  add_subdirectory(tests/native_consumer)
endif()
