#import <UIKit/UIKit.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>
#include "status.h"
#include <mutex>
#include <cstdio>
#include <string>

namespace {
std::mutex logMutex;
std::string visibleLog;
UITextView* statusView;
UIWindow* errorWindow;
std::string directory(NSSearchPathDirectory kind) {
  NSURL* url=[NSFileManager.defaultManager URLsForDirectory:kind inDomains:NSUserDomainMask].firstObject;
  [NSFileManager.defaultManager createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nil];
  return url.path.UTF8String;
}
}
std::string ProbeDocumentsPath() { return directory(NSDocumentDirectory); }
std::string ProbeCachePath() { return directory(NSCachesDirectory); }
void ProbeStatusAppend(const char* text) {
  @autoreleasepool {
    std::string snapshot;
    {
      std::lock_guard lock(logMutex);
      auto path=ProbeDocumentsPath()+"/graphics-log.txt";
      if(auto* file=std::fopen(path.c_str(),"a")) {std::fprintf(file,"%s\n",text); std::fclose(file);}
      visibleLog+=text; visibleLog+='\n';
      if(visibleLog.size()>12000) visibleLog.erase(0,visibleLog.find('\n',visibleLog.size()-10000)+1);
      snapshot=visibleLog;
    }
    std::fprintf(stderr,"%s\n",text);
    NSString* message=[[NSString alloc] initWithBytes:snapshot.data() length:snapshot.size() encoding:NSUTF8StringEncoding];
    dispatch_async(dispatch_get_main_queue(),^{ if(statusView) {
      statusView.text=message;
      [statusView scrollRangeToVisible:NSMakeRange(statusView.text.length,0)];
    }});
  }
}
void ProbeStatusAttach(SDL_Window* window) {
  UIWindow* native=window ? (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr) : nil;
  if(statusView) return;
  if(!native) {
    errorWindow=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    errorWindow.rootViewController=[UIViewController new];
    errorWindow.rootViewController.view.backgroundColor=UIColor.blackColor;
    [errorWindow makeKeyAndVisible];
    native=errorWindow;
  }
  UIView* root=native.rootViewController.view;
  statusView=[[UITextView alloc] initWithFrame:CGRectMake(12,44,root.bounds.size.width-24,200)];
  statusView.autoresizingMask=UIViewAutoresizingFlexibleWidth;
  statusView.editable=NO;
  statusView.backgroundColor=[UIColor colorWithWhite:0 alpha:0.78];
  statusView.textColor=UIColor.whiteColor;
  statusView.font=[UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightRegular];
  statusView.layer.cornerRadius=8;
  [root addSubview:statusView];
  ProbeStatusAppend("Diagnostico grafico GX/Aurora. Sin datos del juego. Log: Archivos/graphics-log.txt");
}
