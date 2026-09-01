# Application executable — owns the UI loop.
add_executable(sturdy_guide_camera main.cpp)
target_link_libraries(
  sturdy_guide_camera
  PRIVATE camera_monitor_lib
)

set_target_properties(
  sturdy_guide_camera
  PROPERTIES
    OUTPUT_NAME sturdy-guide-camera
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
