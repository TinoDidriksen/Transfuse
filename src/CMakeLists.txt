add_executable(transfuse
	base64.hpp
	options.hpp
	html.cpp
	transfuse.cpp
	)
target_include_directories(transfuse PRIVATE ${XXHASH_INCLUDE_DIR} ${ICU_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})
target_link_libraries(transfuse PRIVATE ${XXHASH_LIBRARIES} ${ICU_LIBRARIES} ${ICU_IO_LIBRARIES} ${ICU_I18N_LIBRARIES} ${LIBXML2_LIBRARIES})
