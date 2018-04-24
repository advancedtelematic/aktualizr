set(SOURCES aktualizr_secondary_ipc.cc
            ipuptaneconnection.cc
            ipuptaneconnectionsplitter.cc)

set(HEADERS aktualizr_secondary_ipc.h
            ipuptaneconnection.h
            ipuptaneconnectionsplitter.h)


add_library(aktualizr_secondary_ipc OBJECT ${SOURCES})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})