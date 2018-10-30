set(TARGET_NAME "aktualizr-grpc-srv")

# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Proto file
set(proto_path "${CMAKE_CURRENT_SOURCE_DIR}/proto")
message(STATUS "proto_path: ${proto_path}")
set(proto "${proto_path}/aktualizr.proto")

# Generated sources
set(proto_gen_path "${CMAKE_CURRENT_BINARY_DIR}/proto")
message(STATUS "proto_gen_path: ${proto_gen_path}")
set(proto_srcs "${proto_gen_path}/aktualizr.pb.cc")
set(proto_hdrs "${proto_gen_path}/aktualizr.pb.h")
set(grpc_srcs "${proto_gen_path}/aktualizr.grpc.pb.cc")
set(grpc_hdrs "${proto_gen_path}/aktualizr.grpc.pb.h")

add_custom_command(
      OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${proto_gen_path}"
        --cpp_out "${proto_gen_path}"
        --proto_path "${proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${proto}"
      DEPENDS "${proto}")

include_directories(${proto_gen_path})

set(SOURCES main.cc ${proto_srcs} ${grpc_srcs})

set(HEADERS ${proto_hdrs} ${grpc_hdrs})

aktualizr_source_file_checks(main.cc)

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME} aktualizr_static_lib ${AKTUALIZR_EXTERNAL_LIBS} ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF})

install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin COMPONENT ${TARGET_NAME})

# vim: set tabstop=4 shiftwidth=4 expandtab:
