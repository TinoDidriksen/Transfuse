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

foreach(s tf-extract tf-inject tf-clean)
	add_custom_target(${s} ALL COMMAND ${CMAKE_COMMAND} -E create_symlink transfuse ${s} DEPENDS transfuse)
	install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${s} DESTINATION ${CMAKE_INSTALL_BINDIR})

	add_custom_target(${s}-man ALL COMMAND ${CMAKE_COMMAND} -E create_symlink transfuse.1 ${s}.1)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${s}.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endforeach()

if(BUILD_MAN)
	add_custom_target(man ALL COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/help2man.sh ${PROJECT_VERSION})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/transfuse.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
else()
	install(FILES transfuse.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

install(TARGETS
	transfuse
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
