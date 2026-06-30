set(GSEIM_BUILD_DIR ${PROJECT_BINARY_DIR}/resources/gseim)

file(MAKE_DIRECTORY ${GSEIM_BUILD_DIR})

configure_file(
    slvparms.in
    ${GSEIM_BUILD_DIR}/slvparms.in
    COPYONLY
)

configure_file(
    GSEIM_Library.kicad_sym
    ${GSEIM_BUILD_DIR}/GSEIM_Library.kicad_sym
    COPYONLY
)

file(COPY
    ebe
    xbe
    sub
    DESTINATION ${GSEIM_BUILD_DIR}
)

install(
    FILES
        slvparms.in
        GSEIM_Library.kicad_sym
    DESTINATION ${KICAD_DATA}/resources/gseim
)

install(
    DIRECTORY
        ebe
        xbe
        sub
    DESTINATION ${KICAD_DATA}/resources/gseim
)