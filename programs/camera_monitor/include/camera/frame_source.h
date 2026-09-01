#pragma once

#include <opencv2/core.hpp>

#include <cstddef>

namespace camera {

/// A single captured frame together with a monotonically increasing index.
struct IndexedFrame {
  cv::Mat frame;
  std::size_t index = 0;
};

/// Abstract interface for anything that can produce frames.
///
/// Implementations must be safe to call from a single reader thread; the
/// CameraSession owns the FrameSource and never shares it.
class FrameSource {
 public:
  virtual ~FrameSource() = default;

  /// Return true if the source is ready to produce frames.
  [[nodiscard]] virtual bool is_opened() const = 0;

  /// Read the next frame.
  ///
  /// On success the returned Mat is non-empty.  On failure (end of stream,
  /// device error, simulated failure) the frame is empty or an exception is
  /// thrown — never both silently.
  [[nodiscard]] virtual IndexedFrame read() = 0;
};

}  // namespace camera
