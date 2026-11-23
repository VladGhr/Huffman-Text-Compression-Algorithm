cmake_minimum_required(VERSION 3.16)
project(compressor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(compressor main.cpp)
target_link_libraries(compressor PRIVATE Qt6::Widgets)
# GUI app: no console window behind it on Windows
set_target_properties(compressor PROPERTIES WIN32_EXECUTABLE ON)
target_include_directories(compressor PRIVATE ${CMAKE_SOURCE_DIR}/src)