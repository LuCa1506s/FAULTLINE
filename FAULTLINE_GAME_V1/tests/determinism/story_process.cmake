execute_process(COMMAND "${STORY}" --seed 12345 --fixture-prologue --save story-process-a.flsave
  RESULT_VARIABLE result_a OUTPUT_VARIABLE output_a ERROR_VARIABLE error_a)
execute_process(COMMAND "${STORY}" --seed 12345 --fixture-prologue --save story-process-b.flsave
  RESULT_VARIABLE result_b OUTPUT_VARIABLE output_b ERROR_VARIABLE error_b)
if(NOT result_a STREQUAL "0" OR NOT result_b STREQUAL "0")
  message(FATAL_ERROR "Story failed: ${error_a} ${error_b}")
endif()
file(SHA256 story-process-a.flsave hash_a)
file(SHA256 story-process-b.flsave hash_b)
if(NOT output_a STREQUAL output_b OR NOT hash_a STREQUAL hash_b OR NOT output_a MATCHES "Validation: PASS")
  message(FATAL_ERROR "Story differs between independent processes")
endif()
