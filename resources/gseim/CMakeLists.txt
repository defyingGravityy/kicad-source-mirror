install(
    FILES
        GSEIM_Library.kicad_sym
    DESTINATION ${KICAD_DATA}/symbols
)

install(
    FILES
        sym-lib-table
        slvparms.in
    DESTINATION ${KICAD_DATA}/resources/gseim
)

install(
    DIRECTORY
        ebe
        xbe
    DESTINATION ${KICAD_DATA}/resources/gseim
)