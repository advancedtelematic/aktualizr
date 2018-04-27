include_directories("third_party/jsoncpp")
include_directories("third_party/picojson")

add_subdirectory("utilities")
add_subdirectory("asn1")
add_subdirectory("bootstrap")
add_subdirectory("config")
add_subdirectory("crypto")
add_subdirectory("http")
add_subdirectory("primary")
add_subdirectory("uptane")
add_subdirectory("logging")
add_subdirectory("storage")
add_subdirectory("socket_activation")
add_subdirectory("secondary_ipc")
add_subdirectory("package_manager")
add_subdirectory("telemetry")

if (BUILD_OPCUA)
    add_subdirectory("opcuabridge")
endif (BUILD_OPCUA)

set_source_files_properties(third_party/jsoncpp/jsoncpp.cpp PROPERTIES COMPILE_FLAGS -w)
add_library(jsoncpp OBJECT third_party/jsoncpp/jsoncpp.cpp)

add_library(aktualizr_static_lib STATIC
    $<TARGET_OBJECTS:asn1>
    $<TARGET_OBJECTS:bootstrap>
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:crypto>
    $<TARGET_OBJECTS:http>
    $<TARGET_OBJECTS:jsoncpp>
    $<TARGET_OBJECTS:package_manager>
    $<TARGET_OBJECTS:primary>
    $<TARGET_OBJECTS:utilities>
    $<TARGET_OBJECTS:aktualizr_secondary_ipc>
    $<TARGET_OBJECTS:logging>
    $<TARGET_OBJECTS:storage>
    $<TARGET_OBJECTS:uptane>
    $<TARGET_OBJECTS:socket_activation>
    $<TARGET_OBJECTS:telemetry>)

if (BUILD_OPCUA)
    target_sources(aktualizr_static_lib PRIVATE $<TARGET_OBJECTS:opcua_bridge>)
endif (BUILD_OPCUA)

target_include_directories(aktualizr_static_lib PUBLIC
    $<TARGET_PROPERTY:package_manager,INCLUDE_DIRECTORIES>)

