#pragma once
#include <webgpu/webgpu_cpp.h>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <cstdlib>

namespace aurora {
// Diagnostic policy extracted from the existing fatal GPU behavior for regression tests.
class DiagnosticGpuState {
  mutable std::mutex mutex;
  std::string message;
  std::atomic_bool failed{false};
  bool pending=false;
  uint64_t started=0;
public:
  void Capture(std::string_view text) { std::abort(); }
  std::string Message() const { std::lock_guard lock(mutex); return message; }
  bool Failed() const { return failed.load(std::memory_order_acquire); }
  void InstallCallbacks(wgpu::DeviceDescriptor& desc) {
    desc.SetUncapturedErrorCallback([](const wgpu::Device&,wgpu::ErrorType,wgpu::StringView text,DiagnosticGpuState* self) {
      self->Capture(std::string("WebGPU error: ")+std::string(std::string_view(text)));
    },this);
    desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&,wgpu::DeviceLostReason reason,wgpu::StringView text,DiagnosticGpuState* self) {
        if(reason!=wgpu::DeviceLostReason::Destroyed)
          self->Capture(std::string("Device lost: ")+std::string(std::string_view(text)));
      },this);
  }
  void StartFence(uint64_t now) { pending=true; started=now; }
  void FinishFence() { pending=false; }
  void ResetFence() { pending=false; }
  bool Pending() const { return pending; }
  bool CanRender() const { return !Failed(); }
  void PollTimeout(uint64_t now) { if(pending && now-started>10000) Capture("GPU timeout after 10 active seconds"); }
};
}
