add_subdirectory("utilities")
add_subdirectory("asn1")
add_subdirectory("bootloader")
add_subdirectory("bootstrap")
add_subdirectory("campaign")
add_subdirectory("config")
add_subdirectory("crypto")
add_subdirectory("http")
add_subdirectory("primary")
add_subdirectory("uptane")
add_subdirectory("logging")
add_subdirectory("storage")
add_subdirectory("socket_activation")
add_subdirectory("package_manager")
add_subdirectory("telemetry")
add_subdirectory("isotp_conn")

if (BUILD_OPCUA)
    add_subdirectory("opcuabridge")
endif (BUILD_OPCUA)

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
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:socket_activation>
    $<TARGET_OBJECTS:telemetry>
    $<TARGET_OBJECTS:isotp_conn>)

if (BUILD_OPCUA)
    target_sources(aktualizr_static_lib PRIVATE $<TARGET_OBJECTS:opcua_bridge>)
endif (BUILD_OPCUA)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:package_manager,INCLUDE_DIRECTORIES>)
