cmake_minimum_required(VERSION 3.28)
project(cloudbus-net)

# cloudbus-segment requires at least C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# Get CPM
# For more information on how to add CPM to your project, see: https://github.com/cpm-cmake/CPM.cmake#adding-cpm
include(cmake/CPM.cmake)

# Add stdexec as a dependency
CPMAddPackage(
  NAME stdexec
  GITHUB_REPOSITORY NVIDIA/stdexec
  GIT_TAG main # This will always pull the latest code from the `main` branch. You may also use a specific release version or tag
  OPTIONS
  "STDEXEC_BUILD_TESTS OFF"
  "STDEXEC_BUILD_EXAMPLES OFF"
)

# Add AsyncBerkeley as a dependency.
CPMAddPackage(
  NAME asyncberkeley
  GITHUB_REPOSITORY kcexn/async-berkeley
  GIT_TAG v0.1.2
  OPTIONS
  "IO_BUILD_TESTING OFF"
  "IO_BUILD_EXAMPLES OFF"
)

# Include directories for build and distribution.
set(INCLUDE_DIRS
  "${CMAKE_SOURCE_DIR}/include/"
  "${CMAKE_BINARY_DIR}/_deps/stdexec-src/include/"
  "${CMAKE_BINARY_DIR}/_deps/asyncberkeley-src/include/"
)

# Enable testing by default if this is a top-level project or
# in submodules if the project has explicitly set BUILD_TESTING
# by including CTest.
if ((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME) OR BUILD_TESTING)
	set(CB_NET_BUILD_TESTING_DEFAULT ON)
else()
	set(CB_NET_BUILD_TESTING_DEFAULT OFF)
endif()
option(CB_NET_BUILD_TESTING "Enable testing." ${CB_NET_BUILD_TESTING_DEFAULT})
if (CB_NET_BUILD_TESTING)
  option(CB_NET_ENABLE_COVERAGE "Enable code-coverage." ON)
  if (CB_NET_ENABLE_COVERAGE)
    include(cmake/EnableCoverage.cmake)
  endif()
  # Enable tests.
  include(CTest)
  include(cmake/EnableTests.cmake)
endif()

option(CB_NET_BUILD_DOCS "Build documentation." OFF)
if (CB_NET_BUILD_DOCS)
  include(cmake/EnableDocs.cmake)
endif()
