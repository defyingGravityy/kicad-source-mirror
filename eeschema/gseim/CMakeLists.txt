gseim_paths.cpp

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/ebe
        ${CMAKE_CURRENT_SOURCE_DIR}/xbe
        ${CMAKE_CURRENT_SOURCE_DIR}/sub
    DESTINATION ${KICAD_DATA}/resources
)

install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/slvparms.in
    DESTINATION ${KICAD_DATA}/resources/gseim
)

install(
    PROGRAMS
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/run_gseim
    DESTINATION ${KICAD_DATA}/resources/gseim/bin
)