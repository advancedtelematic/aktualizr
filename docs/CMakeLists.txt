
find_program(INKSCAPE NAMES inkscape)
find_package(Doxygen)

add_custom_target(docs ALL)

if (INKSCAPE)
    add_custom_command(OUTPUT overview.pdf
        COMMAND ${INKSCAPE} --export-pdf=overview.pdf ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/overview.svg)

    add_custom_command(OUTPUT secondary-ecus.pdf
        COMMAND ${INKSCAPE} --export-pdf=secondary-ecus.pdf ${CMAKE_CURRENT_SOURCE_DIR}/secondary-ecus.svg
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/secondary-ecus.svg)

    add_custom_target(docs-inkscape DEPENDS overview.pdf secondary-ecus.pdf)
    add_dependencies(docs docs-inkscape)
endif(INKSCAPE)

if (DOXYGEN_FOUND)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        COMMENT "Generating Doxygen Documentation")
    add_dependencies(docs doxygen)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/ DESTINATION share/doc/aktualizr)
else(DOXYGEN_FOUND)
    message(WARNING "doxygen not found, skipping")
endif(DOXYGEN_FOUND)