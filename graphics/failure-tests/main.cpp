#include <aurora/diagnostic_gpu_state.hpp>
#include "gfx/staging_map.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
  assert(argc==2);
  aurora::DiagnosticGpuState state;
  if(!std::strcmp(argv[1],"ui")) {
    aurora::gfx::StagingMapState mapping;
    auto generation=mapping.request();
    int uiFrames=0;
    auto prepare=[&]{return aurora::gfx::callback_map_ready(mapping);};
    auto beginUi=[&]{++uiFrames;};
    for(int i=0;i<1000;++i) assert(!aurora::PrepareDiagnosticFrame(prepare,beginUi));
    assert(uiFrames==0);
    assert(mapping.complete(generation,aurora::gfx::BufferMapState::Mapped));
    assert(aurora::PrepareDiagnosticFrame(prepare,beginUi) && uiFrames==1);
    puts("UI frame begins only after pending graphics preparation succeeds: PASS");
    return 0;
  }
  if(!std::strcmp(argv[1],"map")) {
    aurora::gfx::StagingMapState mapping;
    auto generation=mapping.request(); assert(generation!=0);
    for(int i=0;i<1000;++i) assert(!aurora::gfx::callback_map_ready(mapping));
    assert(mapping.complete(generation,aurora::gfx::BufferMapState::Mapped));
    assert(aurora::gfx::callback_map_ready(mapping));
    puts("Held staging mapping yields each callback and resumes when ready: PASS");
    return 0;
  }
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
