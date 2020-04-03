# config has to go first, as other libraries append sources to it
add_subdirectory("config")

add_subdirectory("utilities")
add_subdirectory("bootloader")
add_subdirectory("bootstrap")
add_subdirectory("campaign")
add_subdirectory("crypto")
add_subdirectory("http")
add_subdirectory("primary")
add_subdirectory("uptane")
add_subdirectory("logging")
add_subdirectory("storage")
add_subdirectory("package_manager")
add_subdirectory("telemetry")

# deprecated, we recommend using aktualizr_lib
add_library(aktualizr_static_lib STATIC
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:asn1_lib>
    $<TARGET_OBJECTS:bootloader>
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:campaign>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>)

add_library(aktualizr_lib SHARED
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:asn1_lib>
    $<TARGET_OBJECTS:bootloader>
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:campaign>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>)
target_link_libraries(aktualizr_lib ${AKTUALIZR_EXTERNAL_LIBS})
set_target_properties(aktualizr_lib PROPERTIES LIBRARY_OUTPUT_NAME aktualizr)
install(TARGETS aktualizr_lib LIBRARY DESTINATION lib COMPONENT aktualizr)
