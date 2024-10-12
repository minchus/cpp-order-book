cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    cpp-order-book
    VERSION 0.1.0
    DESCRIPTION "C++ order book implementation"
    HOMEPAGE_URL "https://minchus.com"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    cpp-order-book_lib INTERFACE
)

target_include_directories(
    cpp-order-book_lib ${warning_guard}
    INTERFACE
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(cpp-order-book_lib INTERFACE cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(cpp-order-book_lib INTERFACE fmt::fmt)

# ---- Declare executable ----

add_executable(cpp-order-book_exe source/main.cpp)
add_executable(cpp-order-book::exe ALIAS cpp-order-book_exe)

set_property(TARGET cpp-order-book_exe PROPERTY OUTPUT_NAME cpp-order-book)

target_compile_features(cpp-order-book_exe PRIVATE cxx_std_20)

target_link_libraries(cpp-order-book_exe PRIVATE cpp-order-book_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT cpp-order-book_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of cpp-order-book"
  )
endif()

include(cmake/dev-mode.cmake)
