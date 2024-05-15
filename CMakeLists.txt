cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

project(cpp-assignment VERSION 0.1.0)

find_package(flatbuffers REQUIRED)
set(FB_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/property_tree.fbs")
build_flatbuffers("${FB_SCHEMA}" "" fbschemas "" "${CMAKE_CURRENT_SOURCE_DIR}/generated" "" "")
add_library(FlatbuffersTarget INTERFACE)
target_include_directories(FlatbuffersTarget INTERFACE ${FLATBUFFERS_INCLUDE_DIR})
add_dependencies(FlatbuffersTarget fbschemas)

find_package(boost_asio CONFIG REQUIRED)
include_directories(${boost_asio_INCLUDE_DIR})
add_executable(sender sender.cpp)
add_executable(receiver receiver.cpp)
target_link_libraries(sender PRIVATE Boost::asio FlatbuffersTarget)
target_link_libraries(receiver PRIVATE Boost::asio FlatbuffersTarget)
