set(HEADERS socket_activation.h)
if(BUILD_SYSTEMD)
    set(SOURCES socket_activation_systemd.cc)
else()
    set(SOURCES socket_activation_dummy.cc)
endif()

aktualizr_source_file_checks(socket_activation_systemd.cc socket_activation_dummy.cc)

add_library(socket_activation OBJECT ${SOURCES})
target_include_directories(socket_activation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_SYSTEMD)
     target_include_directories(socket_activation PUBLIC ${SYSTEMD_INCLUDE_DIR})
 endif()

aktualizr_source_file_checks(${HEADERS})
