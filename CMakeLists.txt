cmake_minimum_required(VERSION 3.28)
project(
  CppNet
  VERSION 0.5.0
  LANGUAGES CXX)

# net requires at least C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# Get CPM For more information on how to add CPM to your project, see:
# https://github.com/cpm-cmake/CPM.cmake#adding-cpm
include(cmake/CPM.cmake)

# Add stdexec as a dependency
CPMAddPackage(
  URI "gh:NVIDIA/stdexec#main"
  OPTIONS
    "STDEXEC_BUILD_EXAMPLES OFF"
    "STDEXEC_BUILD_TESTING OFF"
)

# Add AsyncBerkeley as a dependency.
CPMAddPackage(
  URI "gh:kcexn/async-berkeley@0.3.1"
  OPTIONS
    "IO_BUILD_TESTING OFF"
    "IO_BUILD_EXAMPLES OFF"
)

# Add cppnet targets.
add_library(cppnet INTERFACE)
target_include_directories(
  cppnet INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
                   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(cppnet INTERFACE asyncberk STDEXEC::stdexec)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Enable testing by default if this is a top-level project or in submodules if
# the project has explicitly set BUILD_TESTING by including CTest.
if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) OR BUILD_TESTING)
  set(CPPNET_BUILD_TESTING_DEFAULT ON)
else()
  set(CPPNET_BUILD_TESTING_DEFAULT OFF)
endif()

option(CPPNET_BUILD_TESTING "Enable testing." ${CPPNET_BUILD_TESTING_DEFAULT})
if(CPPNET_BUILD_TESTING)
  # Add GoogleTest
  message(STATUS "Configure tests with GoogleTest")
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES)
  # For Windows: Prevent overriding the parent project's compiler/linker
  # settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  option(CPPNET_ENABLE_COVERAGE "Enable code-coverage." ON)
  if(CPPNET_ENABLE_COVERAGE)
    include(cmake/EnableCoverage.cmake)
  endif()
  # Enable tests.
  include(CTest)
  add_subdirectory(tests)

  message(STATUS "GoogleTest configured successfully")
endif()

option(CPPNET_BUILD_DOCS "Build documentation." OFF)
if(CPPNET_BUILD_DOCS)
  include(cmake/EnableDocs.cmake)
endif()
