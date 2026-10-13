#include <aurora/diagnostic_gpu_state.hpp>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
  assert(argc==2);
  aurora::DiagnosticGpuState state;
  if(!std::strcmp(argv[1],"fence")) {
    state.StartFence(1000);
    // Submitted work remains pending: no new frame can enter the staging-map wait.
    assert(!state.CanRender());
    for(uint64_t now=1000;now<=11000;++now) {state.PollTimeout(now); assert(!state.Failed());}
    state.PollTimeout(11001);
    assert(state.Failed() && state.Message().find("timeout")!=std::string::npos);
    assert(!state.CanRender());
    puts("Held GPU fence: responsive polling, no extra rendering, timeout persisted PASS");
    return 0;
  }
  auto instance=wgpu::CreateInstance(); assert(instance);
  wgpu::Adapter adapter;
  wgpu::RequestAdapterOptions options{}; options.backendType=wgpu::BackendType::Null;
  instance.RequestAdapter(&options,wgpu::CallbackMode::AllowProcessEvents,
    [&](wgpu::RequestAdapterStatus status,wgpu::Adapter result,wgpu::StringView text) {
      if(status!=wgpu::RequestAdapterStatus::Success) std::fprintf(stderr,"adapter: %.*s\n",int(std::string_view(text).size()),std::string_view(text).data());
      adapter=std::move(result);
    });
  auto wait=[&](auto ready) {
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready() && std::chrono::steady_clock::now()<until) {instance.ProcessEvents(); std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(ready());
  };
  wait([&]{return bool(adapter);});
  wgpu::Device device;
  wgpu::DeviceDescriptor desc{}; state.InstallCallbacks(desc);
  adapter.RequestDevice(&desc,wgpu::CallbackMode::AllowProcessEvents,
    [&](wgpu::RequestDeviceStatus status,wgpu::Device result,wgpu::StringView) { assert(status==wgpu::RequestDeviceStatus::Success); device=std::move(result);});
  wait([&]{return bool(device);});
  if(!std::strcmp(argv[1],"loss")) device.ForceLoss(wgpu::DeviceLostReason::Unknown,"injected device loss");
  else device.InjectError(wgpu::ErrorType::Validation,"injected validation error");
  wait([&]{return state.Failed();});
  assert(!state.CanRender());
  const auto message=state.Message();
  assert(message.find("injected")!=std::string::npos);
  instance.ProcessEvents(); assert(state.Message()==message);
  std::printf("Real Dawn %s callback preserved stopped diagnostic and message: PASS\n",argv[1]);
}
