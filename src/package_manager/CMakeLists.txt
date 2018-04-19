set(SOURCES packagemanagerconfig.cc
            packagemanagerfactory.cc
            packagemanagerfake.cc)

set(HEADERS packagemanagerconfig.h
            packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h)

add_library(package_manager OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})

# Debian backend
if(BUILD_DEB)
    target_sources(package_manager PRIVATE debianmanager.cc)
endif(BUILD_DEB)
aktualizr_source_file_checks(debianmanager.cc debianmanager.h)

# OSTree backend
if(BUILD_OSTREE)
    target_sources(package_manager PRIVATE ostreemanager.cc ostreereposync.cc)
    target_include_directories(package_manager PUBLIC ${LIBOSTREE_INCLUDE_DIRS})
endif(BUILD_OSTREE)
aktualizr_source_file_checks(ostreemanager.cc ostreereposync.cc
                             ostreemanager.h ostreereposync.h)
