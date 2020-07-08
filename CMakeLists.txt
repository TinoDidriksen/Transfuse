cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
project(transfuse
	VERSION 0.1.0
	LANGUAGES CXX
	)
set(VERSION ${PROJECT_VERSION})

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

include(GNUInstallDirs)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /std:c++latest /Zc:__cplusplus /permissive- /W4 /MP")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
	set(_FLAGS_COMMON "-Wall -Wextra -Wno-missing-field-initializers -Wno-deprecated -Wno-unused-parameter -fPIC")

	include(CheckCXXCompilerFlag)

	foreach(flag "-Wno-unused-result" "-flto")
		string(REGEX REPLACE "[^A-Za-z0-9]" "-" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(_FLAGS_COMMON "${_FLAGS_COMMON} ${flag}")
		endif()
	endforeach()
	if(COMPILER_SUPPORTS_flto)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAGS_COMMON} -fvisibility-inlines-hidden")

	# Require latest possible C++ standard
	foreach(flag "-std=c++20" "-std=c++2a" "-std=c++17" "-std=c++1z" "-std=c++14" "-std=c++1y")
		string(REGEX REPLACE "[^a-z0-9]" "-" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
			set(_ENABLED_CXX ${flag})
			break()
		endif()
	endforeach()
	if(NOT _ENABLED_CXX)
		message(FATAL_ERROR "Could not enable at least C++14 (C++1y) - upgrade your compiler")
	endif()
endif()

if(WIN32)
	add_definitions(-D_SECURE_SCL=0 -D_ITERATOR_DEBUG_LEVEL=0 -D_CRT_SECURE_NO_DEPRECATE -DWIN32_LEAN_AND_MEAN -DVC_EXTRALEAN -DNOMINMAX)
endif()

if(VCPKG_TOOLCHAIN)
	find_package(LibXml2 REQUIRED)
	#target_include_directories(main PRIVATE ${LIBXML2_INCLUDE_DIR})
	#target_link_libraries(main PRIVATE ${LIBXML2_LIBRARIES})

	#find_package(sqlite3 CONFIG REQUIRED)
	#target_link_libraries(main PRIVATE sqlite3)

	#find_package(SOCI CONFIG REQUIRED)
	#target_link_libraries(main PRIVATE SOCI::soci_core SOCI::soci_empty SOCI::soci_sqlite3)
else()
	find_package(PkgConfig REQUIRED)

	pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
	#include_directories(${LIBXML2_INCLUDE_DIRS})
	#link_directories(${LIBXML2_LIBRARY_DIRS})

	#pkg_search_module(SQLITE3 REQUIRED sqlite3)
	#include_directories(${SQLITE3_INCLUDE_DIRS})
	#link_directories(${SQLITE3_LIBRARY_DIRS})
endif()

# xxHash
find_path(XXHASH_INCLUDE_DIR xxhash.h REQUIRED)
find_library(XXHASH_LIBRARIES xxhash REQUIRED)

# ICU
find_package(ICU REQUIRED)

enable_testing()

add_subdirectory(src)
