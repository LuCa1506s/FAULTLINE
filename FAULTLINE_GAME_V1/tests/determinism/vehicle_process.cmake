set(a "${CMAKE_CURRENT_BINARY_DIR}/vehicle_a.flsave")
set(b "${CMAKE_CURRENT_BINARY_DIR}/vehicle_b.flsave")
execute_process(COMMAND "${VEHICLES}" --seed 12345 --ticks 240 --save "${a}"
  RESULT_VARIABLE ra OUTPUT_VARIABLE oa ERROR_VARIABLE ea)
execute_process(COMMAND "${VEHICLES}" --seed 12345 --ticks 240 --save "${b}"
  RESULT_VARIABLE rb OUTPUT_VARIABLE ob ERROR_VARIABLE eb)
if(NOT ra EQUAL 0 OR NOT rb EQUAL 0)
  message(FATAL_ERROR "Vehicle process failed: ${ra}/${rb} ${ea} ${eb}")
endif()
if(NOT oa STREQUAL ob)
  message(FATAL_ERROR "Vehicle process stdout differs")
endif()
file(SHA256 "${a}" ha)
file(SHA256 "${b}" hb)
if(NOT ha STREQUAL hb OR NOT oa MATCHES "Vehicles: [1-9]" OR
   NOT oa MATCHES "Police vehicles: [1-9]" OR NOT oa MATCHES "Entered: [1-9]" OR
   NOT oa MATCHES "Abstracted: [1-9]" OR NOT oa MATCHES "Validation: PASS")
  message(FATAL_ERROR "Vehicle process did not exercise deterministic driving/streaming")
endif()
message(STATUS "Vehicle process determinism PASS SHA256=${ha}")
