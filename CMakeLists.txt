cmake_minimum_required(VERSION 3.16)
project(code_snippets_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# Boost is only needed by the two snippets under boost-*/ (boost-copy-filtered,
# boost-quick-allocator). It's optional here: if it isn't installed locally,
# those two targets are skipped with a warning instead of failing the whole
# build -- CI (see .github/workflows/ci.yml) installs libboost-dev and always
# builds them.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW) # Use upstream BoostConfig.cmake, not CMake's bundled FindBoost.
endif()
find_package(Boost QUIET)

include(CTest)
enable_testing()

file(GLOB SNIPPET_SOURCES CONFIGURE_DEPENDS "snippets/*/example.cpp")

foreach(SRC ${SNIPPET_SOURCES})
  get_filename_component(SNIPPET_DIR ${SRC} DIRECTORY)
  get_filename_component(SNIPPET_NAME ${SNIPPET_DIR} NAME)

  # The two Boost-dependent snippets need Boost's headers; skip them cleanly
  # when Boost isn't available instead of failing configure/build.
  if(SNIPPET_NAME MATCHES "^boost-" AND NOT Boost_FOUND)
    message(STATUS "Skipping ${SNIPPET_NAME}: Boost not found locally (CI builds it, see .github/workflows/ci.yml)")
    continue()
  endif()

  set(TARGET_NAME "snippet__${SNIPPET_NAME}")
  add_executable(${TARGET_NAME} ${SRC})
  target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)

  if(SNIPPET_NAME STREQUAL "coroutines-based-task-scheduler")
    # The only C++20 target in the repository -- it uses <coroutine>.
    # Everything else targets C++17 (set above).
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  endif()

  if(SNIPPET_NAME MATCHES "^boost-")
    target_include_directories(${TARGET_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
  endif()

  add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endforeach()
