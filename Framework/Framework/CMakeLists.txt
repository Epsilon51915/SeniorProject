# CMakeList.txt : CMake project for Framework, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Framework "Framework.cpp" "Framework.h" "CommonResources.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Framework PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
