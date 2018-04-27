if(LOGGING_TYPE STREQUAL "boost")
  set(SOURCES boost_logging.cc)
elseif(LOGGING_TYPE STREQUAL "android")
  set(SOURCES android_logging.cc)
else()
  message(FATAL_ERROR "Unknown logger type: ${logger_type}")
endif()

set(HEADERS logging.h)

add_library(logging OBJECT ${SOURCES})
aktualizr_source_file_checks(${SOURCES} ${HEADERS})
