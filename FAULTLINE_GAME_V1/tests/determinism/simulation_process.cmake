execute_process(COMMAND "${SIMULATOR}" --seed 12345 --days 30
  RESULT_VARIABLE result_a OUTPUT_VARIABLE output_a ERROR_VARIABLE error_a)
execute_process(COMMAND "${SIMULATOR}" --seed 12345 --days 30
  RESULT_VARIABLE result_b OUTPUT_VARIABLE output_b ERROR_VARIABLE error_b)
if(NOT result_a STREQUAL "0" OR NOT result_b STREQUAL "0")
  message(FATAL_ERROR "Simulation startup/validation failed: ${result_a}, ${result_b}: ${error_a} ${error_b}")
endif()
if(NOT output_a STREQUAL output_b)
  message(FATAL_ERROR "Simulation state/statistics differ between processes")
endif()
if(NOT output_a MATCHES "Validation: PASS" OR NOT output_a MATCHES "Operations created: [1-9][0-9]*")
  message(FATAL_ERROR "Simulation did not validate or produced no autonomous activity")
endif()

if(NOT output_a MATCHES "NPC semantic updates: [1-9][0-9]*" OR
   NOT output_a MATCHES "Travels completed: [1-9][0-9]*" OR
   NOT output_a MATCHES "NPC operation participations: [1-9][0-9]*" OR
   NOT output_a MATCHES "Relationship changes: [1-9][0-9]*")
  message(FATAL_ERROR "NPC semantic activity/travel/participation/relationships were not exercised")
endif()

if(NOT output_a MATCHES "Control changes: [1-9][0-9]*" OR NOT output_a MATCHES "Contested transitions: [1-9][0-9]*")
  message(FATAL_ERROR "Territory consequences were not exercised")
endif()

if(NOT output_a MATCHES "Market updates: [1-9][0-9]*" OR NOT output_a MATCHES "Market income credited: [1-9][0-9]*")
  message(FATAL_ERROR "Market scheduling/income were not exercised")
endif()
