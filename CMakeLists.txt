cmake_minimum_required(VERSION 2.8)

project(HypergraphGUI)

# Use a .in file to get a correct installation of componentnet library
configure_file(CMakeLists.componentnet.txt.in ext/componentnet/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ext/componentnet" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ext/componentnet" )
add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/componentnet-src"
                 "${CMAKE_CURRENT_BINARY_DIR}/componentnet-build")

set(CMAKE_CXX_STANDARD 11)
include_directories(include)
add_subdirectory(src)
target_include_directories(${PROJECT_NAME} PUBLIC include)
