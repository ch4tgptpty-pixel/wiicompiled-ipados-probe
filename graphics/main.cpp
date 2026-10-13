#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <aurora/diagnostic_gpu_state.hpp>
#include "window.hpp"
#include "webgpu/gpu.hpp"
#include "scene.h"
#include "status.h"
#include <cstdio>
#include <string>
#include <string_view>
#include <cstdlib>
#include <atomic>

namespace {
AuroraInfo info{};
std::string documents,cache;
std::atomic_bool initialized=false;
bool paused=false;
std::atomic_bool failed=false;
bool fenceCompleted=false;
Uint64 preparationStarted=0;
uint32_t frameIndex=0;
uint64_t fenceEpoch=0;
void log(AuroraLogLevel level,const char* module,const char* text,unsigned length) {
  if(level<LOG_INFO) return;
  std::string line=std::string(module ? module : "Aurora")+": "+std::string(text,length);
  ProbeStatusAppend(line.c_str());
  if(level==LOG_FATAL) std::abort();
}
void reportFence(wgpu::QueueWorkDoneStatus status,wgpu::StringView message) {
  aurora::webgpu::diagnostic_gpu_state().FinishFence();
  if(aurora::webgpu::diagnostic_gpu_state().Failed()) return;
  if(status==wgpu::QueueWorkDoneStatus::Success) {
    fenceCompleted=true;
    ProbeStatusAppend("Comandos GPU: PASS. Validacion visual pendiente: debe verse un triangulo multicolor girando.");
  } else {
    std::string line="GPU: FAIL "; line+=std::string(std::string_view(message));
    aurora::webgpu::diagnostic_gpu_state().Capture(line);
  }
}
bool showGpuFailure() {
  const auto message=aurora::webgpu::diagnostic_gpu_state().Message();
  if(message.empty()) return false;
  if(!failed.exchange(true)) ProbeStatusAppend(("GPU: FAIL - "+message+". Reinicia la app.").c_str());
  return true;
}
void preparationPending() {
  const auto now=SDL_GetTicks();
  if(!preparationStarted) preparationStarted=now;
  if(now-preparationStarted>10000) aurora::webgpu::diagnostic_gpu_state().Capture("Preparacion grafica no termino en 10 segundos activos");
  showGpuFailure();
}
}
SDL_AppResult SDL_AppInit(void**,int argc,char** argv) {
  documents=ProbeDocumentsPath(); cache=ProbeCachePath();
  ProbeStatusAppend("Prueba grafica v0.2 GX/Aurora/Dawn para iPadOS. Recorte Retina corregido. No contiene el juego.");
  SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE,"60");
  AuroraConfig config{};
  config.appName="Wii GX Graphics";
  config.userPath=documents.c_str(); config.cachePath=cache.c_str();
  config.pipelineCachePath=cache.c_str();
  config.resourcesPath=SDL_GetBasePath();
  config.desiredBackend=BACKEND_METAL;
  config.msaa=1; config.maxTextureAnisotropy=1;
  config.windowWidth=640; config.windowHeight=480;
  config.pauseOnFocusLost=true;
  config.logCallback=log; config.logLevel=LOG_INFO;
  info=aurora_initialize(argc,argv,&config);
  // Copy SDL-owned text before querying properties or other SDL functions.
  const std::string error=info.initializationError ? info.initializationError : "sin detalle";
  ProbeStatusAttach(info.window);
  if(info.initializationStatus!=AURORA_INITIALIZATION_SUCCESS || info.backend!=BACKEND_METAL) {
    failed=true;
    ProbeStatusAppend(("Inicializacion: FAIL "+error).c_str());
    return SDL_APP_CONTINUE;
  }
  initialized=true;
  if(showGpuFailure()) return SDL_APP_CONTINUE;
  // Shader compilation must not block UIKit; verify readiness before fencing
  // a frame so a skipped first draw cannot be mistaken for scene completion.
  aurora_set_skip_unready_pipelines(true);
  ProbeStatusAppend("Aurora/Dawn: Metal inicializado; GPU pendiente de completar escena.");
  return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppEvent(void*,SDL_Event* event) {
  if(event->type==SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
  if(initialized && !failed) aurora::window::enqueue_callback_event(*event);
  return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void*) {
  if(!initialized || failed) return SDL_APP_CONTINUE;
  if(showGpuFailure()) return SDL_APP_CONTINUE;
  for(const auto* event=aurora_update();event && event->type!=AURORA_NONE;++event) {
    switch(event->type) {
    case AURORA_EXIT: return SDL_APP_SUCCESS;
    case AURORA_PAUSED:
      paused=true; ++fenceEpoch; aurora::webgpu::diagnostic_gpu_state().ResetFence(); fenceCompleted=false; preparationStarted=0;
      ProbeStatusAppend("Ciclo de vida: pausado."); break;
    case AURORA_UNPAUSED:
      paused=false; ++fenceEpoch; aurora::webgpu::diagnostic_gpu_state().ResetFence(); fenceCompleted=false; preparationStarted=0;
      ProbeStatusAppend("Ciclo de vida: reanudado; verificar dibujo y nueva finalizacion GPU."); break;
    case AURORA_WINDOW_RESIZED: case AURORA_DISPLAY_SCALE_CHANGED:
      info.windowSize=event->windowSize;
      ++fenceEpoch; aurora::webgpu::diagnostic_gpu_state().ResetFence(); fenceCompleted=false; preparationStarted=0;
      {char line[120]; std::snprintf(line,sizeof(line),"Superficie: %u x %u pixeles.",info.windowSize.native_fb_width,info.windowSize.native_fb_height); ProbeStatusAppend(line);}
      break;
    default: break;
    }
  }
  aurora::webgpu::g_instance.ProcessEvents();
  auto& gpuState=aurora::webgpu::diagnostic_gpu_state();
  gpuState.PollTimeout(SDL_GetTicks());
  if(showGpuFailure() || paused || !gpuState.CanRender()) return SDL_APP_CONTINUE;
  const bool pipelinesReady=frameIndex>0 && aurora_get_queued_pipeline_count()==0;
  if(frameIndex>0 && !pipelinesReady) { preparationPending(); return SDL_APP_CONTINUE; }
  if(!aurora_begin_frame()) { preparationPending(); return SDL_APP_CONTINUE; }
  preparationStarted=0;
  const auto dimensions=aurora::window::get_window_size();
  const auto drawsBefore=gpuState.EncodedDraws();
  DrawProbeScene(frameIndex++,dimensions.fb_width,dimensions.fb_height);
  aurora_end_frame();
  if(!gpuState.Pending() && !fenceCompleted && gpuState.EncodedDraws()>drawsBefore && !gpuState.Failed()) {
    gpuState.StartFence(SDL_GetTicks());
    const auto submittedEpoch=fenceEpoch;
    aurora::webgpu::g_queue.OnSubmittedWorkDone(wgpu::CallbackMode::AllowProcessEvents,
      [submittedEpoch](wgpu::QueueWorkDoneStatus status,wgpu::StringView message) {
        if(initialized && submittedEpoch==fenceEpoch) reportFence(status,message);
      });
  }
  return SDL_APP_CONTINUE;
}
void SDL_AppQuit(void*,SDL_AppResult) {
  ProbeStatusAppend("Fin de diagnostico grafico.");
  if(initialized.exchange(false) && !failed) aurora_shutdown();
}
