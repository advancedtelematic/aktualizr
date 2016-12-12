# CMake configuration file for subfolder inc/srv

# tell cmake to use the files in this folder for building
set(SOURCES
    ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/oauthtoken.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/servercon.hpp
    PARENT_SCOPE
)
