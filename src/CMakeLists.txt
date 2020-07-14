add_executable(transfuse
	base64.hpp
	dom.hpp
	filesystem.hpp
	options.hpp
	shared.hpp
	state.hpp
	stream.hpp
	string_view.hpp
	xml.hpp

	base64.cpp
	dom.cpp
	extract.cpp
	html.cpp
	inject.cpp
	shared.cpp
	state.cpp
	transfuse.cpp
	)
target_include_directories(transfuse PRIVATE
	${ICU_INCLUDE_DIRS}
	${LIBXML2_INCLUDE_DIRS}
	${LIBZIP_INCLUDE_DIRS}
	${SQLITE3_INCLUDE_DIRS}
	${XXHASH_INCLUDE_DIRS}
	)
target_link_libraries(transfuse PRIVATE
	${ICU_LIBRARIES} ${ICU_IO_LIBRARIES} ${ICU_I18N_LIBRARIES}
	${LIBXML2_LIBRARIES}
	${LIBZIP_LIBRARIES}
	${STDFS_LIB}
	${SQLITE3_LIBRARIES}
	${XXHASH_LIBRARIES}
	)

add_custom_target(tf-extract ALL COMMAND ${CMAKE_COMMAND} -E create_symlink transfuse tf-extract DEPENDS transfuse)
add_custom_target(tf-inject ALL COMMAND ${CMAKE_COMMAND} -E create_symlink transfuse tf-inject DEPENDS transfuse)
add_custom_target(tf-clean ALL COMMAND ${CMAKE_COMMAND} -E create_symlink transfuse tf-clean DEPENDS transfuse)

add_executable(tf-apertium
	filesystem.hpp
	options.hpp

	tf-apertium.cpp
	)
target_link_libraries(tf-apertium PRIVATE
	${STDFS_LIB}
	)
