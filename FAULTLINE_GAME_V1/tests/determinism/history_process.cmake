execute_process(COMMAND "${HISTORY}" --seed 12345 --fixture-prologue --replay
  RESULT_VARIABLE result_a OUTPUT_VARIABLE output_a ERROR_VARIABLE error_a)
execute_process(COMMAND "${HISTORY}" --seed 12345 --fixture-prologue --replay
  RESULT_VARIABLE result_b OUTPUT_VARIABLE output_b ERROR_VARIABLE error_b)
if(NOT result_a STREQUAL "0" OR NOT result_b STREQUAL "0")
  message(FATAL_ERROR "History failed: ${result_a}, ${result_b}: ${error_a} ${error_b}")
endif()
if(NOT output_a STREQUAL output_b)
  message(FATAL_ERROR "History differs between independent processes")
endif()
if(NOT output_a MATCHES "Validation: PASS" OR NOT output_a MATCHES "Months: 48" OR
   NOT output_a MATCHES "Market updates: 1460" OR NOT output_a MATCHES "Replay: PASS")
  message(FATAL_ERROR "History did not verify the full four-year interval")
endif()
