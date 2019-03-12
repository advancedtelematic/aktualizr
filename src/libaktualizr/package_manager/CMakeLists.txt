set(SOURCES packagemanagerconfig.cc
            packagemanagerfactory.cc
            packagemanagerfake.cc
            packagemanagerbase.cc)

set(HEADERS packagemanagerconfig.h
            packagemanagerfactory.h
            packagemanagerfake.h
            packagemanagerinterface.h)

add_library(package_manager OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})

add_aktualizr_test(NAME packagemanagerfake SOURCES packagemanagerfake_test.cc)

# Debian backend
if(BUILD_DEB)
    target_sources(package_manager PRIVATE debianmanager.cc)
    add_executable(t_packagemanager_deb EXCLUDE_FROM_ALL debianmanager_test.cc)
    add_dependencies(build_tests t_packagemanager_deb)
    target_link_libraries(t_packagemanager_deb aktualizr_static_lib ${TEST_LIBS})

    add_test(NAME test_packagemanager_deb COMMAND ${PROJECT_SOURCE_DIR}/tests/run_debian_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/t_packagemanager_deb
            ${PROJECT_SOURCE_DIR}/tests/test_data/fake_dpkg)

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

    add_aktualizr_test(NAME ostree SOURCES ostreemanager_test.cc PROJECT_WORKING_DIRECTORY NO_VALGRIND
                       ARGS ${PROJECT_BINARY_DIR}/ostree_repo)

endif(BUILD_OSTREE)

add_aktualizr_test(NAME packagemanager_factory SOURCES packagemanagerfactory_test.cc NO_VALGRIND
                   ARGS ${PROJECT_BINARY_DIR}/ostree_repo)
add_aktualizr_test(NAME fetcher SOURCES fetcher_test.cc ARGS ${PROJECT_BINARY_DIR}/ostree_repo PROJECT_WORKING_DIRECTORY)
add_aktualizr_test(NAME fetcher_death SOURCES fetcher_death_test.cc NO_VALGRIND ARGS PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(fetcher_death_test.cc fetcher_test.cc)

aktualizr_source_file_checks(packagemanagerfake_test.cc packagemanagerfactory_test.cc ostreemanager_test.cc)

aktualizr_source_file_checks(ostreemanager.cc ostreereposync.cc
                             ostreemanager.h ostreereposync.h)
