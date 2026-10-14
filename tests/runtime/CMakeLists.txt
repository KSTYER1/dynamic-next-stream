cmake_minimum_required(VERSION 3.28)
project(next_stream_review LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
get_filename_component(PLUGIN "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(OBS_DEPS_DIR "${PLUGIN}/.deps" CACHE PATH "Directory containing OBS and Qt development dependencies")
set(OBS "${OBS_DEPS_DIR}/obs-studio-31.1.1")
find_package(Qt6 REQUIRED COMPONENTS Widgets)
# Review-only seams: expose private members and freeze the three wall-clock reads.
# No production source file is modified.
file(READ "${PLUGIN}/src/next-stream-dock.hpp" HEADER)
string(REPLACE "private" "public" HEADER "${HEADER}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/next-stream-dock.hpp" "${HEADER}")
file(READ "${PLUGIN}/src/next-stream-dock.cpp" CPP)
string(REPLACE "time(nullptr)" "review_now()" CPP "${CPP}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/next-stream-dock.cpp" "#include <ctime>\nextern time_t review_now();\n${CPP}")
add_executable(
  next_stream_review
  regression.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/next-stream-dock.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/next-stream-dock.hpp"
)
target_include_directories(
  next_stream_review
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${PLUGIN}/src" "${OBS}/libobs" "${OBS}/frontend/api" "${OBS_DEPS_DIR}/include"
)
target_link_libraries(next_stream_review PRIVATE Qt6::Widgets "${OBS_DEPS_DIR}/lib/obs.lib")
target_compile_options(next_stream_review PRIVATE /utf-8)
