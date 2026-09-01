#pragma once

#include "camera/frame_source.h"

#include <opencv2/core.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

namespace camera {

/// Owns a FrameSource and a worker thread that continuously reads frames.
///
/// ## Thread-safety contract
///
/// 1. `start()`, `stop()`, `join()`, `try_grab_frame()`, and
///    `rethrow_worker_error()` are safe to call from any thread, but only one
///    thread should call `start()`/`stop()`/`join()` — typically the main
///    thread.
///
/// 2. The mutex protects `latest_frame_`, `has_new_frame_`, `stop_requested_`,
///    and `worker_exception_`.  It is **never** held while calling
///    `FrameSource::read()` or performing OpenCV UI / I/O.
///
/// 3. The latest-frame buffer is bounded: when the worker produces a new frame
///    and the consumer has not yet read the previous one, the old frame is
///    discarded.  At most one frame lives in the buffer at any time.
///
/// 4. Worker exceptions are captured via `std::exception_ptr` and can be
///    rethrown on the consumer thread by calling `rethrow_worker_error()`.
///
/// ## State machine
///
///     Idle ──start()──▸ Running ──stop()──▸ Stopping ──worker exits──▸ Idle
///
///   - `start()` from Idle moves to Running and spawns the worker.
///   - `stop()` from Running moves to Stopping and signals the worker.
///   - `join()` blocks until the worker exits and moves back to Idle.
///   - Calling `start()` while already running is a no-op (returns false).
///   - Calling `stop()` or `join()` when not running is a safe no-op.
class CameraSession {
 public:
  /// Take ownership of @p source.
  explicit CameraSession(std::unique_ptr<FrameSource> source);
  ~CameraSession();

  CameraSession(const CameraSession&) = delete;
  CameraSession& operator=(const CameraSession&) = delete;

  /// Start the worker thread.  Returns true if the transition happened, false
  /// if already running.
  bool start();

  /// Signal the worker to stop.  Idempotent — safe to call multiple times.
  void stop();

  /// Block until the worker thread has exited.  Safe to call even if the
  /// session was never started or was already joined.
  void join();

  /// Non-blocking: if a new frame is available, move it into @p out and return
  /// true.  Otherwise return false.
  bool try_grab_frame(cv::Mat& out);

  /// If the worker captured an exception, rethrow it on the calling thread.
  /// Does nothing if no error occurred.
  void rethrow_worker_error();

 private:
  void worker_main() noexcept;

  std::unique_ptr<FrameSource> source_;

  // -- Shared state protected by mu_ ----------------------------------------
  std::mutex mu_;
  cv::Mat latest_frame_;
  bool has_new_frame_ = false;
  bool stop_requested_ = false;
  std::exception_ptr worker_exception_;
  // -------------------------------------------------------------------------

  std::condition_variable frame_cv_;
  std::thread worker_;
  std::atomic<bool> running_{false};
};

}  // namespace camera
