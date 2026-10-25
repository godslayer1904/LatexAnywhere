cmake_minimum_required(VERSION 3.21)
project(LatexAnywhere LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /W3)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# QuickJS-ng：只要库本体
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/quickjs EXCLUDE_FROM_ALL)

add_library(la_core STATIC core/native/js_engine.cpp)
target_include_directories(la_core PUBLIC core/native third_party/quickjs)
target_link_libraries(la_core PUBLIC qjs)

add_executable(la_compile tools/la_compile.cpp)
target_link_libraries(la_compile PRIVATE la_core)

add_executable(la_difftest tests/diff/difftest.cpp)
target_link_libraries(la_difftest PRIVATE la_core)
