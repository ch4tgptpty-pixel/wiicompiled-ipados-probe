#include <aurora/callback_sdl_events.hpp>
#include <cassert>
#include <thread>
#include <cstdio>
int main() {
  aurora::CallbackSdlEvents events;
  SDL_Event resize{}; resize.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
  resize.window.data1=2048; resize.window.data2=1536;
  SDL_Event background{}; background.type=SDL_EVENT_DID_ENTER_BACKGROUND;
  SDL_Event quit{}; quit.type=SDL_EVENT_QUIT;
  assert(events.Push(resize)); assert(events.Push(background)); assert(events.Push(quit));
  auto batch=events.Drain();
  assert(batch.size()==3 && batch[0].window.data1==2048 && batch[1].type==background.type && batch[2].type==quit.type);
  assert(events.Drain().empty());
  SDL_Event drop{}; drop.type=SDL_EVENT_DROP_FILE; drop.drop.data="expired after callback";
  assert(!events.Push(drop));
  SDL_Event text{}; text.type=SDL_EVENT_TEXT_INPUT; text.text.text="also borrowed";
  assert(!events.Push(text)); assert(events.Drain().empty());
  std::thread producer([&]{for(int i=0;i<100;++i){resize.window.data1=i; assert(events.Push(resize));}});
  producer.join(); batch=events.Drain(); assert(batch.size()==100);
  for(int i=0;i<100;++i) assert(batch[i].window.data1==i);
  puts("Callback lifecycle events ordered, drained once, borrowed payloads excluded, cross-thread producer: PASS");
}
