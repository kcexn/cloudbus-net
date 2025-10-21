# EnableDocs.cmake - Configure Doxygen documentation for cloudbus-segment.
#
# This module configures Doxygen documentation generation when CB_SEGMENT_BUILD_DOCS is ON.
# It handles Doxygen configuration, creates docs targets, and sets up GitHub Pages deployment.
message(STATUS "Configuring Doxygen documentation")

# Find Doxygen
find_package(Doxygen REQUIRED)
if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen not found. Please install doxygen to build documentation.")
endif()

file(GLOB_RECURSE
  DOXYGEN_INPUT_FILES_STR
  "${CMAKE_SOURCE_DIR}/include/*.hpp"
)
list(APPEND DOXYGEN_INPUT_FILES_STR "${CMAKE_SOURCE_DIR}/README.md")
string(REPLACE ";" " " DOXYGEN_INPUT_FILES "${DOXYGEN_INPUT_FILES_STR}")

# Set input and output directories
set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/docs/Doxyfile)
set(DOXYGEN_OUT ${CMAKE_SOURCE_DIR}/docs/html)

# Request to configure the file
configure_file(${DOXYGEN_IN} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

# Add custom target for documentation generation
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
)

message(STATUS "Doxygen documentation enabled. Use 'cmake --build . --target docs' to generate documentation.")
