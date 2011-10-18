cmake_minimum_required (VERSION 2.8.1)
project (RTSServer)
cmake_policy(VERSION 2.8.1)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(CMAKE_CXX_FLAGS "-g -Wall")

set(RTSSERVER_ROOT ".")

include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories("${RTSSERVER_ROOT}/include")

# MAIN PROGRAM
file (GLOB_RECURSE CPPFILES src/*.cpp)

set(LUA_LIBRARY liblua.a)
add_executable(
   RTSServer
   src/LuaTest.cpp
)
target_link_libraries(RTSServer ${LUA_LIBRARY} ${CMAKE_DL_LIBS})
#find_package (Lua51)
#include_directories(${LUA51_INCLUDE_DIRS})
#target_link_libraries (RTSServer ${LUA51_LIBRARIES})
