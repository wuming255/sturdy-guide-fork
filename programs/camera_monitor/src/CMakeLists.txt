# Core library — hardware-independent, shared by app and tests.
add_library(camera_monitor_lib
  open_cv_camera.cpp
  fake_frame_source.cpp
  camera_session.cpp
)
add_library(CameraMonitor::camera_monitor_lib ALIAS camera_monitor_lib)

target_compile_features(camera_monitor_lib PUBLIC cxx_std_17)
target_include_directories(
  camera_monitor_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(Threads REQUIRED)
target_link_libraries(
  camera_monitor_lib
  PUBLIC ${OpenCV_LIBS} Threads::Threads
)

if(STURDY_GUIDE_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(camera_monitor_lib PRIVATE /W4)
  else()
    target_compile_options(
      camera_monitor_lib
      PRIVATE -Wall -Wextra -Wpedantic
    )
  endif()
endif()
