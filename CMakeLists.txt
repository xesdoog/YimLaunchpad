cmake_minimum_required(VERSION 3.15)

project(YLP LANGUAGES CXX)

include(FetchContent)
include(cmake/imgui.cmake)

set(CMAKE_CXX_STANDARD 20)
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
file(GLOB_RECURSE SRC_FILES
    "${SRC_DIR}/**.hpp"
    "${SRC_DIR}/**.cpp"
)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_precompile_headers(${PROJECT_NAME} PUBLIC "${SRC_DIR}/common.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui OpenGL::GL)
add_compile_definitions("_CRT_SECURE_NO_WARNINGS" "NOMINMAX" "WIN32_LEAN_AND_MEAN")

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
