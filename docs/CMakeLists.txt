
find_program(INKSCAPE NAMES inkscape)

if (INKSCAPE)
    add_custom_command(OUTPUT overview.pdf
        COMMAND ${INKSCAPE} --export-pdf=overview.pdf ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg)

    add_custom_command(OUTPUT secondary-ecus.pdf
        COMMAND ${INKSCAPE} --export-pdf=secondary-ecus.pdf ${CMAKE_CURRENT_SOURCE_DIR}/secondary-ecus.svg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/secondary-ecus.svg)

    add_custom_target(docs ALL DEPENDS overview.pdf secondary-ecus.pdf)
endif(INKSCAPE)
