gseim_paths.cpp

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/ebe
        ${CMAKE_CURRENT_SOURCE_DIR}/xbe
    DESTINATION ${KICAD_DATA}/resources
)

install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/slvparms.in
    DESTINATION ${KICAD_DATA}/resources/gseim
)