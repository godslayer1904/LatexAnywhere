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

# ---------------- 引擎字节码（Windows、Linux 共用） ----------------
set(LA_VERSION "1.0.0")
if(WIN32 OR (UNIX AND NOT APPLE))
  # 默认用本次构建的 la_compile 生成；交叉编译时由 -DLA_COMPILER= 指定能在本机运行的 la_compile
  set(LA_BYTECODE_DIR "${CMAKE_BINARY_DIR}/bytecode")
  if(LA_COMPILER)
    set(_compiler "${LA_COMPILER}")
    set(_dep "")
  else()
    set(_compiler $<TARGET_FILE:la_compile>)
    set(_dep la_compile $<TARGET_FILE:la_compile>)   # 依赖可执行文件本身：la_compile 变了就重新生成字节码
  endif()
  add_custom_command(OUTPUT ${LA_BYTECODE_DIR}/engine.qbc ${LA_BYTECODE_DIR}/math.qbc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LA_BYTECODE_DIR}
    COMMAND ${_compiler} ${CMAKE_SOURCE_DIR}/core/js/dist/engine.js ${LA_BYTECODE_DIR}/engine.qbc
    COMMAND ${_compiler} ${CMAKE_SOURCE_DIR}/core/mathjax/dist/math.js ${LA_BYTECODE_DIR}/math.qbc
    DEPENDS ${_dep} ${CMAKE_SOURCE_DIR}/core/js/dist/engine.js ${CMAKE_SOURCE_DIR}/core/mathjax/dist/math.js
    COMMENT "Compiling engine bytecode")
endif()

# ---------------- Windows 程序 ----------------
if(WIN32)
  string(REPLACE "." "," LA_VER_COMMA "${LA_VERSION},0")
  set(LA_SRC "${CMAKE_SOURCE_DIR}")
  configure_file(win/app.rc.in ${CMAKE_BINARY_DIR}/app.rc @ONLY)
  set_source_files_properties(${CMAKE_BINARY_DIR}/app.rc PROPERTIES OBJECT_DEPENDS "${LA_BYTECODE_DIR}/engine.qbc;${LA_BYTECODE_DIR}/math.qbc")
  add_executable(LatexAnywhere WIN32 win/main.cpp win/box.cpp win/help.cpp win/i18n.cpp win/sysio.cpp win/preview.cpp win/util.cpp ${CMAKE_BINARY_DIR}/app.rc)
  target_include_directories(LatexAnywhere PRIVATE win)
  target_compile_definitions(LatexAnywhere PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_options(LatexAnywhere PRIVATE /MANIFEST:NO)
  target_link_libraries(LatexAnywhere PRIVATE la_core uxtheme d2d1 dwrite dwmapi imm32 oleacc comctl32 dbghelp psapi shlwapi shell32 ole32 oleaut32 uuid)
endif()

# ---------------- Linux 程序（GTK4 + Wayland/X11） ----------------
if(UNIX AND NOT APPLE)
  enable_language(ASM)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LA_DEPS REQUIRED IMPORTED_TARGET gtk4 librsvg-2.0 atspi-2)
  set_source_files_properties(linux/res.S PROPERTIES
    COMPILE_DEFINITIONS "LA_ENGINE_QBC=\"${LA_BYTECODE_DIR}/engine.qbc\";LA_MATH_QBC=\"${LA_BYTECODE_DIR}/math.qbc\";LA_ICON_SVG=\"${CMAKE_SOURCE_DIR}/linux/res/latexanywhere.svg\""
    OBJECT_DEPENDS "${LA_BYTECODE_DIR}/engine.qbc;${LA_BYTECODE_DIR}/math.qbc;${CMAKE_SOURCE_DIR}/linux/res/latexanywhere.svg")
  add_executable(latexanywhere linux/main.cpp linux/box.cpp linux/help.cpp linux/sysio.cpp linux/tray.cpp linux/util.cpp win/i18n.cpp linux/res.S)
  target_include_directories(latexanywhere PRIVATE linux win)
  target_compile_options(latexanywhere PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wno-missing-field-initializers -Wno-bidi-chars>)
  target_link_libraries(latexanywhere PRIVATE la_core PkgConfig::LA_DEPS)
endif()
