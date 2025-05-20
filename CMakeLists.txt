cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
project(transfuse
	VERSION 0.7.3
	LANGUAGES CXX C
	)

option(BUNDLED_XXHASH "Set to ON to use the bundled xxHash instead of host's version" OFF)
option(BUILD_DOCS "Set to ON to build the documentation" OFF)
option(BUILD_MAN "Set to ON to build man page" OFF)

set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(MASTER_PROJECT ON)
endif ()

# Release or Debug
if(MASTER_PROJECT AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

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

	foreach(flag "-Wconversion" "-flto")
		string(REGEX REPLACE "[^A-Za-z0-9]" "" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(_FLAGS_COMMON "${_FLAGS_COMMON} ${flag}")
		endif()
	endforeach()
	if(COMPILER_SUPPORTS_flto)
		set(CMAKE_AR "gcc-ar")
		set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
		set(CMAKE_CXX_ARCHIVE_FINISH true)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAGS_COMMON} -fvisibility-inlines-hidden")

	# Require latest possible C++ standard
	foreach(flag "-std=c++20" "-std=c++2a" "-std=c++17")
		string(REGEX REPLACE "[^a-z0-9]" "" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
			set(_ENABLED_CXX ${flag})
			break()
		endif()
	endforeach()
	if(NOT _ENABLED_CXX)
		message(FATAL_ERROR "Could not enable at least C++17 - upgrade your compiler")
	endif()
endif()

include(TestBigEndian)
test_big_endian(ARCH_BIG_ENDIAN)
if(ARCH_BIG_ENDIAN)
	add_definitions(-DARCH_BIG_ENDIAN)
endif()

if(WIN32)
	add_definitions(-D_SECURE_SCL=0 -D_ITERATOR_DEBUG_LEVEL=0 -D_CRT_SECURE_NO_DEPRECATE -DWIN32_LEAN_AND_MEAN -DVC_EXTRALEAN -DNOMINMAX)
endif()

if(VCPKG_TOOLCHAIN)
	# libxml2
	find_package(LibXml2 REQUIRED)
	# sqlite3
	find_path(SQLITE3_INCLUDE_DIRS sqlite3.h REQUIRED)
	find_library(SQLITE3_LIBRARIES sqlite3 REQUIRED)
else()
	find_package(PkgConfig REQUIRED)
	pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
	pkg_search_module(SQLITE3 REQUIRED sqlite3)
endif()

# libzip, using pkg-config fails for MacPorts
find_path(LIBZIP_INCLUDE_DIRS zip.h PATH_SUFFIXES libzip REQUIRED)
find_library(LIBZIP_LIBRARIES zip REQUIRED)

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(filesystem HAS_FS)
if(HAS_FS)
	add_definitions(-DHAS_FS)
endif()

# std::filesystem is in a separate library until g++ 9
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
	set(STDFS_LIB stdc++fs)
endif()

# ICU
find_package(ICU REQUIRED)

enable_testing()

add_subdirectory(include/xxhash)
add_subdirectory(src)
add_subdirectory(tests)

if(BUILD_DOCS)
	add_subdirectory(docs)
endif()
