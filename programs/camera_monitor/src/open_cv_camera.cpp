#include "camera/open_cv_camera.h"

#include <stdexcept>

namespace camera {

OpenCvCamera::OpenCvCamera(const int device_index, const int width,
                           const int height) {
  capture_.open(device_index, cv::CAP_ANY);
  if (!capture_.isOpened()) {
    throw std::runtime_error(
        "cannot open camera; check the device index, permissions, and "
        "whether another program is using it");
  }
  capture_.set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(width));
  capture_.set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(height));
}

bool OpenCvCamera::is_opened() const { return capture_.isOpened(); }

IndexedFrame OpenCvCamera::read() {
  cv::Mat frame;
  if (!capture_.read(frame) || frame.empty()) {
    // Returning an empty frame signals the session to stop.
    return {cv::Mat{}, next_index_};
  }
  return {std::move(frame), next_index_++};
}

}  // namespace camera
