add_subdirectory(libjwt)
add_library(rvi STATIC
	src/btree.c src/rvi.c src/rvi_list.c
)
target_include_directories(rvi PUBLIC include)
target_include_directories(rvi PRIVATE libjwt/include)
target_link_libraries(rvi jwt_static crypto ssl jansson)
