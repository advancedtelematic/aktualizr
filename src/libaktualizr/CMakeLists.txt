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

if(BUILD_ISOTP)
    add_subdirectory("isotp_conn")
endif(BUILD_ISOTP)

add_library(aktualizr_static_lib STATIC
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

if (BUILD_ISOTP)
    target_sources(aktualizr_static_lib PRIVATE $<TARGET_OBJECTS:isotp_conn>)
endif (BUILD_ISOTP)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:package_manager,INCLUDE_DIRECTORIES>)
