# Camera Monitor — Refactored

## 构建

```bash
cmake -S . -B build-camera -G Ninja \
  -DSTURDY_GUIDE_BUILD_CAMERA_HOMEWORK=ON
cmake --build build-camera
```

## 运行

```bash
./build-camera/sturdy-guide-camera [--device 0] [--width 1280] [--height 720]
```

按键：`S` 截图，`Q` 退出。

## 测试

```bash
ctest --test-dir build-camera --output-on-failure
```

所有测试使用 `FakeFrameSource`，不需要真实摄像头。

## 架构

```text
FrameSource (抽象接口)
├── OpenCvCamera        cv::VideoCapture 访问真实设备
└── FakeFrameSource     测试用确定性假帧

CameraSession
├── 独占一个 FrameSource (unique_ptr)
├── worker thread 持续读取帧
├── 维护 latest-frame 缓冲区（最多一帧）
└── 通过 exception_ptr 将后台异常传回主线程

PreviewApplication (app/main.cpp)
├── 主线程调用 cv::imshow / cv::waitKey
├── 处理截图、退出、FPS 叠加
└── 不直接访问 CameraSession 内部 mutex
```

## 线程安全契约

1. **资源所有权**：`CameraSession` 通过 `unique_ptr<FrameSource>` 拥有帧来源。`cv::VideoCapture` 由 `OpenCvCamera` 独占。worker thread 由 `CameraSession` 创建和管理。latest frame 由 mutex 保护。

2. **状态机**：`Idle → start() → Running → stop() → Stopping → worker exits → Idle`。`stop()` 幂等，`join()` 在非运行态为空操作。

3. **线程安全方法**：`start()`、`stop()`、`join()`、`try_grab_frame()`、`rethrow_worker_error()` 可从任意线程调用，但 start/stop/join 应由同一线程（主线程）调用。

4. **mutex 保护**：`mu_` 保护 `latest_frame_`、`has_new_frame_`、`stop_requested_`、`worker_exception_`。不变量：任意时刻缓冲区至多一帧。

5. **丢帧策略**：worker 产生新帧时，若消费者未读取旧帧，旧帧被覆盖（latest-frame 语义）。消费者永远看到最新画面。

6. **不持锁调用慢 I/O**：`FrameSource::read()` 在锁外执行，避免阻塞主线程的 `try_grab_frame()`。同理，`imshow()`/`imwrite()` 在主线程执行，不持有 session 的 mutex。

7. **异常传播**：worker 通过 `std::exception_ptr` 捕获异常，存入 `worker_exception_`，主线程通过 `rethrow_worker_error()` 重新抛出。

8. **析构顺序**：`~CameraSession()` 调用 `stop()`（设置标志 + 唤醒）→ `join()`（等待 worker 退出）→ 销毁成员。
