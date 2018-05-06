set(SOURCES aktualizr_secondary_ipc.cc
            ipuptaneconnection.cc
            ipuptaneconnectionsplitter.cc)

set(HEADERS aktualizr_secondary_ipc.h
            ipuptaneconnection.h
            ipuptaneconnectionsplitter.h)


add_library(aktualizr_secondary_ipc OBJECT ${SOURCES})

add_aktualizr_test(NAME ipuptane_connection SOURCES ipuptaneconnection_test.cc PROJECT_WORKING_DIRECTORY)

aktualizr_source_file_checks(${SOURCES} ${HEADERS} ${TEST_SOURCES})