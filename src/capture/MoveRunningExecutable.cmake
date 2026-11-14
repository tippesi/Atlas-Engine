# Moves the executable aside before it is linked: a running capture tool keeps running from the renamed file and the
# new one can be written. Renamed files of earlier builds are removed once they don't run anymore
file(GLOB previous "${EXECUTABLE}.old-*")
foreach(file IN LISTS previous)
    execute_process(COMMAND ${CMAKE_COMMAND} -E rm -f "${file}" RESULT_VARIABLE ignored ERROR_QUIET)
endforeach()

if (EXISTS "${EXECUTABLE}")
    string(TIMESTAMP stamp "%Y%m%d%H%M%S")
    file(RENAME "${EXECUTABLE}" "${EXECUTABLE}.old-${stamp}" RESULT renamed)
endif()
