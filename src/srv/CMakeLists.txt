# CMake configuration file for subfolder src/srv

set(SOURCES
    ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/oauthtoken.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/servercon.cpp
    PARENT_SCOPE
)
