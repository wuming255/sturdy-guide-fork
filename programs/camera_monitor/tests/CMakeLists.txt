# Hardware-independent tests using FakeFrameSource.
add_executable(camera_session_test camera_session_test.cpp)
target_link_libraries(
  camera_session_test
  PRIVATE camera_monitor_lib
)
add_test(NAME camera_session_test COMMAND camera_session_test)
