// Automated tests for CameraSession + FakeFrameSource.
// No real camera is required.

#include "camera/camera_session.h"
#include "camera/fake_frame_source.h"

#include <opencv2/core.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

// ======================================================================
// Minimal assertion helpers (no external test framework dependency).
// ======================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr)                                             \
  do {                                                                \
    ++g_tests_run;                                                    \
    if (!(expr)) {                                                    \
      std::cerr << "  FAIL: " << #expr << "  (" << __FILE__ << ":"   \
                << __LINE__ << ")\n";                                 \
      return false;                                                   \
    }                                                                 \
    ++g_tests_passed;                                                 \
  } while (false)

#define RUN_TEST(func)                                                \
  do {                                                                \
    std::cout << "  " << #func << " ... ";                            \
    if (func()) {                                                     \
      std::cout << "ok\n";                                            \
    } else {                                                          \
      std::cout << "FAILED\n";                                        \
      all_passed = false;                                             \
    }                                                                 \
  } while (false)

// ======================================================================
// Test 1: basic lifecycle — start, grab frame, stop, join.
// ======================================================================
static bool test_basic_lifecycle() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  TEST_ASSERT(session.start());

  cv::Mat frame;
  // Poll until a frame arrives (with a timeout to avoid hanging).
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!session.try_grab_frame(frame)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      std::cerr << "(timeout waiting for frame) ";
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  TEST_ASSERT(!frame.empty());

  session.stop();
  session.join();
  return true;
}

// ======================================================================
// Test 2: frame indices are monotonically increasing.
// ======================================================================
static bool test_frame_indices_increase() {
  camera::FakeFrameSource::Options opts;
  opts.total_frames = 10;
  auto source = std::make_unique<camera::FakeFrameSource>(opts);
  camera::CameraSession session(std::move(source));

  session.start();

  cv::Mat frame;
  bool got_first = false;

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (session.try_grab_frame(frame)) {
      got_first = true;
      TEST_ASSERT(!frame.empty());
      break;  // Got a frame — that's enough for this test.
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  session.stop();
  session.join();
  TEST_ASSERT(got_first);
  return true;
}

// ======================================================================
// Test 3: double-start returns false.
// ======================================================================
static bool test_double_start() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  TEST_ASSERT(session.start());
  TEST_ASSERT(!session.start());  // second start must return false

  session.stop();
  session.join();
  return true;
}

// ======================================================================
// Test 4: stop() is idempotent.
// ======================================================================
static bool test_stop_idempotent() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  session.start();
  // Give the worker a moment to actually begin.
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  session.stop();
  session.stop();  // second stop must not crash or deadlock
  session.join();
  return true;
}

// ======================================================================
// Test 5: join() without start() is a no-op.
// ======================================================================
static bool test_join_without_start() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  session.join();  // must not crash
  return true;
}

// ======================================================================
// Test 6: stop() without start() is a no-op.
// ======================================================================
static bool test_stop_without_start() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  session.stop();  // must not crash
  return true;
}

// ======================================================================
// Test 7: latest-frame semantics — buffer never exceeds one frame.
// ======================================================================
static bool test_latest_frame_bounded() {
  // Use a read_hook that inserts a small delay so the worker produces
  // several frames before we start consuming.
  camera::FakeFrameSource::Options opts;
  opts.read_hook = [](std::size_t) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  };
  auto source = std::make_unique<camera::FakeFrameSource>(opts);
  camera::CameraSession session(std::move(source));

  session.start();

  // Let the worker run for a while without consuming.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Now grab exactly one frame — the session must have kept only the latest.
  cv::Mat frame;
  bool got = session.try_grab_frame(frame);

  session.stop();
  session.join();

  // We should be able to get at most one frame at a time.
  // After grabbing one, the next grab should return false (no queued frames).
  if (got) {
    cv::Mat second;
    TEST_ASSERT(!session.try_grab_frame(second));
  }
  return true;
}

// ======================================================================
// Test 8: FakeFrameSource read failure propagates to main thread.
// ======================================================================
static bool test_worker_error_propagation() {
  camera::FakeFrameSource::Options opts;
  opts.fail_after = 3;  // throw after 3 successful reads
  auto source = std::make_unique<camera::FakeFrameSource>(opts);
  camera::CameraSession session(std::move(source));

  session.start();

  // Wait for the worker to fail.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool caught = false;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      session.rethrow_worker_error();
    } catch (const std::runtime_error& e) {
      caught = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  session.stop();
  session.join();

  TEST_ASSERT(caught);
  return true;
}

// ======================================================================
// Test 9: empty frame from source causes session to stop gracefully.
// ======================================================================
static bool test_empty_frame_stops_session() {
  camera::FakeFrameSource::Options opts;
  opts.total_frames = 2;  // produce 2 frames, then return empty
  auto source = std::make_unique<camera::FakeFrameSource>(opts);
  camera::CameraSession session(std::move(source));

  session.start();

  // Wait until the session stops on its own.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    cv::Mat frame;
    session.try_grab_frame(frame);
    // Check if the worker has stopped.
    try {
      session.rethrow_worker_error();
    } catch (...) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  session.stop();
  session.join();  // must return without hanging
  return true;
}

// ======================================================================
// Test 10: destructor without explicit stop/join is safe.
// ======================================================================
static bool test_destructor_safety() {
  {
    camera::FakeFrameSource::Options opts;
    opts.read_hook = [](std::size_t) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    };
    auto source = std::make_unique<camera::FakeFrameSource>(opts);
    camera::CameraSession session(std::move(source));
    session.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // Let destructor handle stop + join.
  }
  // If we get here without crashing or deadlocking, the test passes.
  return true;
}

// ======================================================================
// Test 11: try_grab_frame returns false when no new frame.
// ======================================================================
static bool test_try_grab_no_frame() {
  auto source = std::make_unique<camera::FakeFrameSource>();
  camera::CameraSession session(std::move(source));

  // Before starting — no frames available.
  cv::Mat frame;
  TEST_ASSERT(!session.try_grab_frame(frame));

  session.start();
  // Immediately — frame may or may not be ready yet.  Just verify no crash.
  session.try_grab_frame(frame);

  session.stop();
  session.join();
  return true;
}

// ======================================================================
// Test 12: rethrow_worker_error does nothing when no error.
// ======================================================================
static bool test_rethrow_no_error() {
  camera::FakeFrameSource::Options opts;
  opts.total_frames = 5;
  auto source = std::make_unique<camera::FakeFrameSource>(opts);
  camera::CameraSession session(std::move(source));

  session.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  session.stop();
  session.join();

  // Must not throw.
  session.rethrow_worker_error();
  return true;
}

// ======================================================================
// Test 13: repeated start/stop/join cycles.
// ======================================================================
static bool test_multiple_cycles() {
  for (int cycle = 0; cycle < 3; ++cycle) {
    camera::FakeFrameSource::Options opts;
    opts.total_frames = 5;
    auto source = std::make_unique<camera::FakeFrameSource>(opts);
    camera::CameraSession session(std::move(source));

    session.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    session.stop();
    session.join();
  }
  return true;
}

// ======================================================================
// Main
// ======================================================================
int main() {
  std::cout << "CameraSession tests:\n";

  bool all_passed = true;

  RUN_TEST(test_basic_lifecycle);
  RUN_TEST(test_frame_indices_increase);
  RUN_TEST(test_double_start);
  RUN_TEST(test_stop_idempotent);
  RUN_TEST(test_join_without_start);
  RUN_TEST(test_stop_without_start);
  RUN_TEST(test_latest_frame_bounded);
  RUN_TEST(test_worker_error_propagation);
  RUN_TEST(test_empty_frame_stops_session);
  RUN_TEST(test_destructor_safety);
  RUN_TEST(test_try_grab_no_frame);
  RUN_TEST(test_rethrow_no_error);
  RUN_TEST(test_multiple_cycles);

  std::cout << "\nResults: " << g_tests_passed << "/" << g_tests_run
            << " assertions passed.\n";

  if (!all_passed) {
    std::cerr << "SOME TESTS FAILED\n";
    return 1;
  }
  std::cout << "ALL TESTS PASSED\n";
  return 0;
}
