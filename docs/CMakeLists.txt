
find_package(Doxygen)

add_custom_target(docs)

if (DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        COMMENT "Generating Doxygen Documentation")
    add_dependencies(docs doxygen)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/ DESTINATION share/doc/aktualizr OPTIONAL)
else(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
    message(WARNING "doxygen + dot not found, skipping")
endif(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
