if(NOT DEFINED READELF OR NOT EXISTS "${READELF}")
    message(FATAL_ERROR "readelf is unavailable: ${READELF}")
endif()
if(NOT DEFINED WRAPPER OR NOT EXISTS "${WRAPPER}")
    message(FATAL_ERROR "native wrapper is unavailable: ${WRAPPER}")
endif()
if(NOT DEFINED PLAIN OR NOT EXISTS "${PLAIN}")
    message(FATAL_ERROR "plain library is unavailable: ${PLAIN}")
endif()

get_filename_component(plain_name "${PLAIN}" NAME)
execute_process(
        COMMAND "${READELF}" --dynamic "${WRAPPER}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE dynamic_section
        ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "readelf failed for ${WRAPPER}: ${error}")
endif()

string(FIND "${dynamic_section}" "Shared library: [${plain_name}]" dependency_position)
if(dependency_position EQUAL -1)
    message(FATAL_ERROR "${WRAPPER} does not have DT_NEEDED for ${plain_name}:\n${dynamic_section}")
endif()
