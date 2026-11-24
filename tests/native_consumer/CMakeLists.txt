cmake_minimum_required(VERSION 3.20)

if(NOT FAULTLINE_SDK_EMBEDDED_TEST)
  project(FaultlineExternalConsumer LANGUAGES CXX)
  enable_testing()
  set(FAULTLINE_CHECKOUT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "FAULTLINE source checkout")
  add_subdirectory("${FAULTLINE_CHECKOUT}" faultline EXCLUDE_FROM_ALL)
endif()

add_executable(faultline_native_consumer main.cpp)
target_link_libraries(faultline_native_consumer PRIVATE faultline::sdk)
add_test(NAME framework_native_consumer COMMAND faultline_native_consumer)
set_tests_properties(framework_native_consumer PROPERTIES LABELS "framework;fast;determinism;persistence" TIMEOUT 120)
