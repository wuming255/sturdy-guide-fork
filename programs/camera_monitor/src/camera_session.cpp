#include "camera/camera_session.h"

#include <utility>

namespace camera {

CameraSession::CameraSession(std::unique_ptr<FrameSource> source)
    : source_(std::move(source)) {}

CameraSession::~CameraSession() {
  // Ensure the worker is stopped and joined before we destroy members.
  stop();
  join();
}

bool CameraSession::start() {
  if (running_.exchange(true)) {
    return false;  // Already running.
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    stop_requested_ = false;
    worker_exception_ = nullptr;
    has_new_frame_ = false;
    latest_frame_ = cv::Mat{};
  }
  worker_ = std::thread(&CameraSession::worker_main, this);
  return true;
}

void CameraSession::stop() {
  if (!running_.load()) {
    return;  // Not running — nothing to do.
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    stop_requested_ = true;
  }
  frame_cv_.notify_all();
}

void CameraSession::join() {
  if (worker_.joinable()) {
    worker_.join();
  }
  running_.store(false);
}

bool CameraSession::try_grab_frame(cv::Mat& out) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!has_new_frame_) {
    return false;
  }
  out = latest_frame_;  // shallow copy — caller gets their own Mat header
  has_new_frame_ = false;
  return true;
}

void CameraSession::rethrow_worker_error() {
  std::exception_ptr err;
  {
    std::lock_guard<std::mutex> lk(mu_);
    err = worker_exception_;
  }
  if (err) {
    std::rethrow_exception(err);
  }
}

// -----------------------------------------------------------------------
// Worker thread — runs in its own thread, never touches OpenCV UI.
// -----------------------------------------------------------------------
void CameraSession::worker_main() noexcept {
  try {
    for (;;) {
      // ---- Check stop flag (lock-free fast path first) ----
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (stop_requested_) {
          return;
        }
      }

      // ---- Read frame OUTSIDE the lock ----
      IndexedFrame indexed = source_->read();

      // ---- Publish frame under the lock ----
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (stop_requested_) {
          return;
        }
        if (!indexed.frame.empty()) {
          // Latest-frame semantics: overwrite the old frame unconditionally.
          // The consumer only cares about the most recent image.
          latest_frame_ = indexed.frame;  // shallow copy
          has_new_frame_ = true;
        } else {
          // Empty frame signals end-of-stream or device error.
          stop_requested_ = true;
        }
      }
      frame_cv_.notify_all();
    }
  } catch (...) {
    std::lock_guard<std::mutex> lk(mu_);
    worker_exception_ = std::current_exception();
    stop_requested_ = true;
    frame_cv_.notify_all();
  }
}

}  // namespace camera
