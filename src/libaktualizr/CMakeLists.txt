set(LIBAKTUALIZR_PUBLIC_HEADERS
    ../../include/libaktualizr/config.h
    ../../include/libaktualizr/types.h
    ../../include/libaktualizr/events.h
    ../../include/libaktualizr/results.h
    ../../include/libaktualizr/campaign.h
    ../../include/libaktualizr/secondaryinterface.h
    ../../include/libaktualizr/secondary_provider.h
    ../../include/libaktualizr/packagemanagerinterface.h
    ../../include/libaktualizr/packagemanagerfactory.h)

aktualizr_source_file_checks(${LIBAKTUALIZR_PUBLIC_HEADERS})

# note: the config object is composed with multiple sub-config objects that live
# close to the modules they correspond to. To make the config module as
# self-contained as possible, the method definitions of these sub objects are
# also added to the module with CMake `target_sources(config PRIVATE ...)`
# declarations.
# config has to go first, as other libraries append sources to it
add_library(config OBJECT)

add_subdirectory("bootloader")
add_subdirectory("bootstrap")
add_subdirectory("campaign")
add_subdirectory("config")
add_subdirectory("crypto")
add_subdirectory("http")
add_subdirectory("logging")
add_subdirectory("package_manager")
add_subdirectory("primary")
add_subdirectory("storage")
add_subdirectory("telemetry")
add_subdirectory("uptane")
add_subdirectory("utilities")

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
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary_config>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:utilities>)

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
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary_config>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:utilities>)

target_link_libraries(aktualizr_lib ${AKTUALIZR_EXTERNAL_LIBS})
set_target_properties(aktualizr_lib PROPERTIES LIBRARY_OUTPUT_NAME aktualizr)
install(TARGETS aktualizr_lib LIBRARY DESTINATION lib COMPONENT aktualizr)
