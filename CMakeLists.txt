cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(programmer_idea)

include(cmake/AddFolderSubdirectories.cmake)
include(cmake/AddSourceGroups.cmake)

#set (BOOST_ROOT C:/local/boost_1_62_0)
#set (BOOST_INCLUDEDIR ${BOOST_ROOT}/boost)
#set (BOOST_LIBRARYDIR ${BOOST_ROOT}/lib64-msvc-12.0)

set (Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${BOOST_INCLUDEDIR})
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${BOOST_LIBRARYDIR})

find_package(Boost REQUIRED system thread date_time regex serialization)


if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
endif()

link_directories(${Boost_LIBRARY_DIRS})  
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${root_SOURCE_DIR} ./)

add_subdirectory(compiler)

