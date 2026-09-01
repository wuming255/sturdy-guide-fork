#include "camera/fake_frame_source.h"

#include <opencv2/imgproc.hpp>

namespace camera {

FakeFrameSource::FakeFrameSource() = default;
FakeFrameSource::FakeFrameSource(Options opts) : opts_(std::move(opts)) {}

bool FakeFrameSource::is_opened() const { return opened_; }

IndexedFrame FakeFrameSource::read() {
  if (opts_.read_hook) {
    opts_.read_hook(next_index_);
  }

  // Simulate a permanent device failure.
  if (opts_.fail_after > 0 && next_index_ >= opts_.fail_after) {
    throw std::runtime_error("FakeFrameSource: simulated read failure at frame " +
                             std::to_string(next_index_));
  }

  // Simulate end-of-stream: produce empty frames after the limit.
  if (opts_.total_frames > 0 && next_index_ >= opts_.total_frames) {
    opened_ = false;
    return {cv::Mat{}, next_index_};
  }

  // Generate a deterministic frame: solid colour whose value equals the index
  // modulo 256, with a fixed small size.
  cv::Mat frame(opts_.height, opts_.width, CV_8UC3,
                cv::Scalar(static_cast<double>(next_index_ % 256), 100, 100));
  return {frame, next_index_++};
}

}  // namespace camera
