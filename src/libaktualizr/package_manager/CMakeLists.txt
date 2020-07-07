set(SOURCES packagemanagerfactory.cc
            packagemanagerfake.cc
            packagemanagerinterface.cc)

set(HEADERS packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h)

add_library(package_manager OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} packagemanagerconfig.cc ${HEADERS})

target_sources(config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/packagemanagerconfig.cc)

add_aktualizr_test(NAME packagemanagerfake SOURCES packagemanagerfake_test.cc LIBRARIES PUBLIC uptane_generator_lib)

# Debian backend
if(BUILD_DEB)
    set_property(SOURCE packagemanagerfactory.cc packagemanagerfactory_test.cc PROPERTY COMPILE_DEFINITIONS BUILD_DEB)
    target_sources(package_manager PRIVATE debianmanager.cc)

    add_aktualizr_test(NAME debianmanager SOURCES debianmanager_test.cc)
    set_tests_properties(test_debianmanager PROPERTIES ENVIRONMENT PATH=${PROJECT_SOURCE_DIR}/tests/test_data/fake_dpkg:$ENV{PATH})

endif(BUILD_DEB)
aktualizr_source_file_checks(debianmanager.cc debianmanager.h debianmanager_test.cc)

# OSTree backend
if(BUILD_OSTREE)
    target_sources(package_manager PRIVATE ostreemanager.cc ostreereposync.cc)
    target_include_directories(package_manager PUBLIC ${LIBOSTREE_INCLUDE_DIRS})

    add_custom_target(make_ostree_sysroot
                      COMMAND ${PROJECT_SOURCE_DIR}/scripts/make_ostree_sysroot.sh
                      ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
    add_dependencies(build_tests make_ostree_sysroot)

    add_aktualizr_test(NAME ostreemanager SOURCES ostreemanager_test.cc PROJECT_WORKING_DIRECTORY
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo)

    if(BUILD_DOCKERAPP)
        target_sources(package_manager PRIVATE dockerappmanager.cc dockerapp_standalone.cc dockerapp_bundles.cc)
        add_aktualizr_test(NAME dockerapp SOURCES dockerappmanager_test.cc PROJECT_WORKING_DIRECTORY
                           ARGS ${PROJECT_BINARY_DIR}/ostree_repo "$<TARGET_FILE:uptane-generator>")
    endif(BUILD_DOCKERAPP)
endif(BUILD_OSTREE)

add_aktualizr_test(NAME packagemanager_factory SOURCES packagemanagerfactory_test.cc
                   ARGS ${PROJECT_BINARY_DIR}/ostree_repo)
add_aktualizr_test(NAME fetcher SOURCES fetcher_test.cc ARGS PROJECT_WORKING_DIRECTORY LIBRARIES PUBLIC uptane_generator_lib)
add_aktualizr_test(NAME fetcher_death SOURCES fetcher_death_test.cc NO_VALGRIND ARGS PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(fetcher_death_test.cc fetcher_test.cc)

aktualizr_source_file_checks(packagemanagerfake_test.cc packagemanagerfactory_test.cc ostreemanager_test.cc)

aktualizr_source_file_checks(ostreemanager.cc ostreereposync.cc
                             ostreemanager.h ostreereposync.h)

aktualizr_source_file_checks(androidmanager.cc androidmanager.h)
aktualizr_source_file_checks(dockerappmanager.cc dockerappmanager.h dockerapp_standalone.cc dockerapp_bundles.cc dockerappmanager_test.cc)

if(ANDROID)
    target_sources(package_manager PRIVATE androidmanager.cc)
endif(ANDROID)
