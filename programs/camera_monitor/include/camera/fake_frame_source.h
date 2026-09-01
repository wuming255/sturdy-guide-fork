#pragma once

#include "camera/frame_source.h"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>

namespace camera {

/// A fully deterministic FrameSource for automated tests.
///
/// Behaviour is configurable:
///  - `total_frames` controls how many frames are produced before returning
///    empty frames (default: unlimited).
///  - `fail_after` makes `read()` throw after that many successful frames.
///  - `read_hook` is called before each read — tests can inject delays or
///    custom logic.
class FakeFrameSource : public FrameSource {
 public:
  struct Options {
    /// Number of valid frames to produce.  0 means unlimited.
    std::size_t total_frames = 0;
    /// Throw std::runtime_error after this many successful reads.  0 = never.
    std::size_t fail_after = 0;
    /// Optional width / height for generated frames.
    int width = 64;
    int height = 48;
    /// Called before each read().  Useful for injecting delays or sync points.
    std::function<void(std::size_t /*next_index*/)> read_hook;
  };

  FakeFrameSource();
  explicit FakeFrameSource(Options opts);

  [[nodiscard]] bool is_opened() const override;
  [[nodiscard]] IndexedFrame read() override;

 private:
  Options opts_;
  std::size_t next_index_ = 0;
  bool opened_ = true;
};

}  // namespace camera
