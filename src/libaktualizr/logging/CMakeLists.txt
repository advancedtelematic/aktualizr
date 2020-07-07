set(SOURCES logging.cc logging_config.cc)

if(ANDROID)
  list(APPEND SOURCES android_log_sink.cc)
else()
  list(APPEND SOURCES default_log_sink.cc)
endif()

set(HEADERS logging.h)

add_library(logging OBJECT ${SOURCES})
aktualizr_source_file_checks(logging.cc logging_config.cc android_log_sink.cc default_log_sink.cc ${HEADERS})
