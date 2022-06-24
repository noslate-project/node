project(MetaCache)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_BUILD_TYPE Debug)

#add_definitions(-std=c++11)
#set(CMAKE_CXX_STANDARD 11)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

add_library(mc_preload SHARED 
    ld_preload.c 
)

#add_executable(
#  test_metacache 
#  test_main.cc
#)

add_executable(
  test_avltree
  test_avl.cc
)

add_executable(
  test_writer
  test_writer.cc
)

add_executable(
  test_hash
  test_hash.cc
  city.cc
)

add_executable(
  test_kv
  test_kv.cc
)


#target_link_libraries(
#  test_metacache 
#  libmetacache 
#)

#target_link_libraries(
#  test_avltree 
#  libmetacache 
#)
