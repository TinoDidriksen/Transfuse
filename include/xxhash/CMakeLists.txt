if(NOT BUNDLED_XXHASH)
	find_path(XXHASH_INCLUDE_DIR xxhash.h)
	find_library(XXHASH_LIBRARIES xxhash)
	set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIRS} PARENT_SCOPE)
	set(XXHASH_LIBRARIES ${XXHASH_LIBRARIES} PARENT_SCOPE)
else()
	add_library(xxhash STATIC
		xxhash.h
		xxh3.h
		xxhash.cpp
		)
	target_include_directories(xxhash PRIVATE ./)
	set(XXHASH_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
	set(XXHASH_LIBRARIES xxhash PARENT_SCOPE)
endif()
