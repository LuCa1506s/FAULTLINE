foreach(version RANGE 1 3)
  execute_process(COMMAND "${WORLDGEN}" --seed 12345 --version ${version} --validate
    RESULT_VARIABLE result_a OUTPUT_VARIABLE output_a ERROR_VARIABLE error_a)
  execute_process(COMMAND "${WORLDGEN}" --seed 12345 --version ${version} --validate
    RESULT_VARIABLE result_b OUTPUT_VARIABLE output_b ERROR_VARIABLE error_b)
  if(NOT result_a STREQUAL "0" OR NOT result_b STREQUAL "0")
    message(FATAL_ERROR "Worldgen v${version} startup/validation failed: ${result_a}, ${result_b}: ${error_a} ${error_b}")
  endif()
  if(NOT output_a STREQUAL output_b)
    message(FATAL_ERROR "Worldgen v${version} differs across processes")
  endif()
endforeach()
