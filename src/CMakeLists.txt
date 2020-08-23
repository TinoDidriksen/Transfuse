configure_file(config.hpp.in config.hpp @ONLY)

add_executable(transfuse
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
	base64.hpp
	dom.hpp
	formats.hpp
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
	format-docx.cpp
	format-html.cpp
	format-html-fragment.cpp
	format-odt.cpp
	format-pptx.cpp
	format-text.cpp
	inject.cpp
	shared.cpp
	state.cpp
	stream-apertium.cpp
	stream-visl.cpp
	transfuse.cpp
	)
target_include_directories(transfuse PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
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

install(TARGETS
	transfuse
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
install(PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/tf-extract
	${CMAKE_CURRENT_BINARY_DIR}/tf-inject
	${CMAKE_CURRENT_BINARY_DIR}/tf-clean
	DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
