
find_program(INKSCAPE NAMES inkscape)

if (INKSCAPE)
    add_custom_command(OUTPUT overview.pdf
        COMMAND ${INKSCAPE} --export-pdf=overview.pdf ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg)

    add_custom_target(docs ALL DEPENDS overview.pdf)
endif(INKSCAPE)