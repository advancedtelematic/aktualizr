set(SOURCES initialize.cc
    ipuptanesecondary.cc
    legacysecondary.cc
    managedsecondary.cc
    partialverificationsecondary.cc
    role.cc
    root.cc
    tuf.cc
    uptanerepository.cc
    virtualsecondary.cc)

set(HEADERS exceptions.h
    ipuptanesecondary.h
    legacysecondary.h
    managedsecondary.h
    partialverificationsecondary.h
    secondaryconfig.h
    secondaryfactory.h
    secondaryinterface.h
    tuf.h
    uptanerepository.h
    virtualsecondary.h)

add_library(uptane OBJECT ${SOURCES})

aktualizr_source_file_checks(${SOURCES} ${HEADERS})
