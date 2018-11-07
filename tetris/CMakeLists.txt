cmake_minimum_required(VERSION 3.5)
project(tetris)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS
    -g
    -Wall
    -Werror
)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include(FindPkgConfig)
PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)

add_executable(${PROJECT_NAME} tetris.cpp)
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
