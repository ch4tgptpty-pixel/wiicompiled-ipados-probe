#import <UIKit/UIKit.h>
#include "ios_memory_tests.h"
#include "guest_flat_memory.h"
#include <sys/utsname.h>
#include <unistd.h>
#include <sstream>
bool RunIosMemoryAccessTests(std::function<void(const std::string&)> report);
int MkwContextTestMain();

@interface MemoryController : UIViewController
@property(nonatomic,strong) UITextView* output;
@property(nonatomic,strong) UIButton* run;
@end
@implementation MemoryController
- (void)append:(NSString*)line {
    self.output.text=[self.output.text stringByAppendingFormat:@"%@\n",line];
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor=UIColor.systemBackgroundColor;
    self.output=[UITextView new]; self.output.editable=NO;
    self.output.font=[UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular];
    self.run=[UIButton buttonWithType:UIButtonTypeSystem];
    [self.run setTitle:@"Ejecutar pruebas (una vez)" forState:UIControlStateNormal];
    [self.run addTarget:self action:@selector(start) forControlEvents:UIControlEventTouchUpInside];
    UIStackView* stack=[[UIStackView alloc] initWithArrangedSubviews:@[self.run,self.output]];
    stack.axis=UILayoutConstraintAxisVertical; stack.spacing=12; stack.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:stack];
    UILayoutGuide* safe=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
      [stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
      [stack.topAnchor constraintEqualToAnchor:safe.topAnchor constant:12],
      [stack.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-12],
      [self.run.heightAnchor constraintEqualToConstant:48]]];
    struct utsname device; uname(&device);
    [self append:@"Validación v0.3 del backend real; no contiene el juego."];
    [self append:[NSString stringWithFormat:@"Dispositivo: %s; iPadOS: %@; página: %d bytes",
      device.machine,UIDevice.currentDevice.systemVersion,getpagesize()]];
    [self append:@"Se probarán MEM1 24 MiB, MEM2 128 MiB y sus espejos.\nSi se cierra, informa el último resultado visible."];
}
- (void)start {
    self.run.enabled=NO;
    [self append:@"Iniciando pruebas del mapa completo…"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        auto report=[self](const std::string& line) {
            NSString* text=[[NSString alloc] initWithBytes:line.data() length:line.size() encoding:NSUTF8StringEncoding];
            dispatch_async(dispatch_get_main_queue(), ^{ [self append:text]; });
        };
        if(!RunIosMemoryTests(report)) return;
        std::ostringstream detail;
        detail<<"Base dinámica: 0x"<<std::hex<<GuestFlat::g_flatGuestBase
              <<"; acceso comprobado: "<<(GuestFlat::RequiresCheckedAccess()?"sí":"no");
        report(detail.str());
        if(!RunIosMemoryAccessTests(report)) return;
        report("Iniciando cambios de contexto originales…");
        report(MkwContextTestMain()==0 ? "Contexto original: PASS" : "Contexto original: FAIL");
        report("Fin de pruebas. El juego y sus gráficos siguen sin validar.");
    });
}
@end
@interface MemoryDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation MemoryDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options {
    self.window=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController=[MemoryController new];
    [self.window makeKeyAndVisible]; return YES;
}
@end
int main(int argc,char** argv) { @autoreleasepool { return UIApplicationMain(argc,argv,nil,NSStringFromClass(MemoryDelegate.class)); } }
