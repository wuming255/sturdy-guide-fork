#pragma once

#include "camera/frame_source.h"

#include <opencv2/videoio.hpp>

#include <string>

namespace camera {

/// FrameSource backed by cv::VideoCapture for real hardware.
class OpenCvCamera : public FrameSource {
 public:
  /// Open the device at @p device_index, requesting @p width × @p height.
  ///
  /// Throws std::runtime_error if the device cannot be opened.
  OpenCvCamera(int device_index, int width = 1280, int height = 720);

  [[nodiscard]] bool is_opened() const override;

  /// Read a frame.  Returns an empty Mat on transient failure; throws
  /// std::runtime_error when the device permanently stops producing frames.
  [[nodiscard]] IndexedFrame read() override;

 private:
  cv::VideoCapture capture_;
  std::size_t next_index_ = 0;
};

}  // namespace camera
