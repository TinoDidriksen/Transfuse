add_executable(transfuse
	base64.hpp
	dom.hpp
	formats.hpp
	filesystem.hpp
	options.hpp
	shared.hpp
	state.hpp
	stream.hpp
	stream-apertium.hpp
	stream-visl.hpp
	string_view.hpp
	xml.hpp

	base64.cpp
	dom.cpp
	extract.cpp
	format-html.cpp
	format-html-fragment.cpp
	format-odt.cpp
	format-text.cpp
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

add_executable(tf-apertium-spread tf-apertium-spread.cpp)
add_executable(tf-apertium-close tf-apertium-close.cpp)

install(TARGETS
	transfuse
	tf-apertium-spread
	tf-apertium-close
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
install(PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/tf-extract
	${CMAKE_CURRENT_BINARY_DIR}/tf-inject
	${CMAKE_CURRENT_BINARY_DIR}/tf-clean
	DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
