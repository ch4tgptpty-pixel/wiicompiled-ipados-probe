#pragma once
#include <SDL3/SDL_events.h>
#include <mutex>
#include <vector>

namespace aurora {
// Lifecycle/window-only bridge for the synthetic iOS callback application.
// Drop/text payloads are borrowed from SDL and must never outlive AppEvent.
class CallbackSdlEvents {
  std::mutex mutex;
  std::vector<SDL_Event> pending;
public:
  bool Push(const SDL_Event& event) {
    bool allowed=event.type>=SDL_EVENT_WINDOW_FIRST && event.type<=SDL_EVENT_WINDOW_LAST;
    switch(event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WILL_ENTER_BACKGROUND: case SDL_EVENT_DID_ENTER_BACKGROUND:
      case SDL_EVENT_WILL_ENTER_FOREGROUND: case SDL_EVENT_DID_ENTER_FOREGROUND:
        allowed=true; break;
      default: break;
    }
    if(!allowed) return false;
    std::lock_guard lock(mutex);
    pending.push_back(event);
    return true;
  }
  std::vector<SDL_Event> Drain() {
    std::lock_guard lock(mutex);
    std::vector<SDL_Event> batch;
    batch.swap(pending);
    return batch;
  }
};
}
