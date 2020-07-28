foreach(f docx html pptx odt)
	add_test(NAME "extract-${f}" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/extract.sh $<TARGET_FILE:transfuse> "${CMAKE_CURRENT_SOURCE_DIR}" ${f})
endforeach()
