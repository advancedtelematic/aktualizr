set(SOURCES packagemanagerfactory.cc
            packagemanagerfake.cc
)

set(HEADERS packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h)

if(BUILD_DEB)
    list(APPEND SOURCES debianmanager.cc)
    list(APPEND HEADERS debianmanager.h)
else(BUILD_DEB)
    aktualizr_source_file_checks(debianmanager.cc debianmanager.h)
endif(BUILD_DEB)

if(BUILD_OSTREE)
    list(APPEND SOURCES ostreemanager.cc ostreereposync.cc)
    list(APPEND HEADERS ostreemanager.h ostreereposync.h)

else(BUILD_OSTREE)
    aktualizr_source_file_checks(ostreemanager.cc ostreereposync.cc 
                                 ostreemanager.h ostreereposync.h)
endif(BUILD_OSTREE)

add_library(package_manager OBJECT ${SOURCES})

if(BUILD_OSTREE)
    target_include_directories(package_manager PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
endif(BUILD_OSTREE)

aktualizr_source_file_checks(${SOURCES} ${HEADERS})