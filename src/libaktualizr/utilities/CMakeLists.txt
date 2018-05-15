set(SOURCES timer.cc
            types.cc
            utils.cc)

set(HEADERS channel.h
            config_utils.h
            exceptions.h
            timer.h
            types.h
            utils.h)


add_library(utilities OBJECT ${SOURCES})
include(AddAktualizrTest)

add_aktualizr_test(NAME timer SOURCES timer_test.cc)
add_aktualizr_test(NAME utils SOURCES utils_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})

