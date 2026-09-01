# Camera homework — OpenCV is required.
find_package(
  OpenCV 4
  REQUIRED
  COMPONENTS core highgui imgcodecs imgproc videoio
)

# Keep the original starter for reference.
add_subdirectory(starter)

# Refactored library, application, and tests.
add_subdirectory(src)
add_subdirectory(app)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
