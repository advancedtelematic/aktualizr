set(SOURCES campaign.cc)

add_library(campaign OBJECT ${SOURCES})

add_aktualizr_test(NAME campaign SOURCES campaign_test.cc ARGS ${CMAKE_CURRENT_SOURCE_DIR}/test)

aktualizr_source_file_checks(${SOURCES} ${TEST_SOURCES})
