// Disposable feasibility probe. This is not the game or the Aurora renderer.
#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include <mach/mach.h>
#include <sys/utsname.h>
#include "host_context.h"
#include "guest_flat_memory.h"

namespace {
HostContext::Handle scheduler = nullptr;
HostContext::Handle worker = nullptr;
int steps = 0;
bool identitiesMatch = true;

void Worker(void*) {
  identitiesMatch &= HostContext::IsCurrent(worker);
  ++steps;
  HostContext::Switch(scheduler);
  identitiesMatch &= HostContext::IsCurrent(worker);
  ++steps;
  HostContext::Switch(scheduler);
}

bool TestContext() {
  steps = 0;
  identitiesMatch = true;
  if (!HostContext::InitializeScheduler(&scheduler)) return false;
  identitiesMatch &= HostContext::IsCurrent(scheduler);
  worker = HostContext::Create(64 * 1024, Worker, nullptr);
  if (!worker) {
    HostContext::ShutdownScheduler(scheduler);
    scheduler = nullptr;
    return false;
  }
  HostContext::Switch(worker);
  bool ok = steps == 1 && HostContext::IsCurrent(scheduler);
  HostContext::Switch(worker);
  ok &= steps == 2 && HostContext::IsCurrent(scheduler) && identitiesMatch;
  HostContext::Destroy(worker);
  HostContext::ShutdownScheduler(scheduler);
  worker = scheduler = nullptr;
  return ok;
}
} // namespace

@interface ProbeController : UIViewController <MTKViewDelegate>
@property(nonatomic, strong) UITextView *report;
@property(nonatomic, strong) id<MTLCommandQueue> queue;
@property(nonatomic) BOOL submitted;
- (void)append:(NSString *)message;
@end

@implementation ProbeController
- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.systemBackgroundColor;
  UIStackView *stack = [[UIStackView alloc] init];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 12;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stack];
  UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
    [stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
    [stack.topAnchor constraintEqualToAnchor:safe.topAnchor constant:16],
    [stack.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-16]]];

  UILabel *title = [[UILabel alloc] init];
  title.text = @"WiiCompiled · diagnóstico para iPad";
  title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  title.numberOfLines = 0;
  [stack addArrangedSubview:title];
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  MTKView *metal = [[MTKView alloc] initWithFrame:CGRectZero device:device];
  metal.clearColor = MTLClearColorMake(0.08, 0.28, 0.62, 1.0);
  metal.paused = YES;
  metal.enableSetNeedsDisplay = YES;
  metal.delegate = self;
  self.queue = [device newCommandQueue];
  [metal.heightAnchor constraintEqualToConstant:120].active = YES;
  [stack addArrangedSubview:metal];

  self.report = [[UITextView alloc] init];
  self.report.text = @"";
  self.report.editable = NO;
  self.report.font = [UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular];
  [stack addArrangedSubview:self.report];
  struct utsname info = {};
  uname(&info);
  [self append:[NSString stringWithFormat:@"Prototipo desechable; no contiene el juego.\nDispositivo: %s\niPadOS: %@\nPágina VM: %zu bytes\nMetal: %@\n",
    info.machine, UIDevice.currentDevice.systemVersion, (size_t)vm_page_size,
    device ? device.name : @"no disponible"]];
  [self append:@"Metal: pendiente de completar un comando GPU.\nLas pruebas de memoria y contexto se ejecutan por separado.\n"];
  [self append:@"Diagnóstico v0.2: reserva fija, reserva dinámica y aliases.\n"];

  NSArray<NSString *> *names = @[@"Probar reserva virtual", @"Probar contexto del runtime", @"Copiar informe"];
  NSArray<NSString *> *selectors = @[@"testMemory", @"testContext", @"copyReport"];
  for (NSUInteger i = 0; i < names.count; ++i) {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:names[i] forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [button addTarget:self action:NSSelectorFromString(selectors[i]) forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:button];
  }
  [metal setNeedsDisplay];
}
- (void)append:(NSString *)message {
  self.report.text = [self.report.text stringByAppendingString:message];
  NSLog(@"%@", message);
  NSURL *documents = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
  [self.report.text writeToURL:[documents URLByAppendingPathComponent:@"probe-report.txt"]
                   atomically:YES encoding:NSUTF8StringEncoding error:nil];
}
- (void)testMemory {
  // A virtual reservation only; no 4 GiB RAM fill and no overwrite of existing mappings.
  // vm_allocate is used because the macOS mach_vm_* declarations may be absent in the iOS SDK.
  vm_address_t address = (vm_address_t)GuestFlat::kFixedFlatGuestBase;
  const vm_size_t size = (vm_size_t)GuestFlat::kGuestSpaceSize;
  kern_return_t result = vm_allocate(mach_task_self(), &address, size, VM_FLAGS_FIXED);
  [self append:[NSString stringWithFormat:@"Reserva 4 GiB en 0x%llx: %s (%d).\n",
    (unsigned long long)GuestFlat::kFixedFlatGuestBase, mach_error_string(result), result]];
  if (result == KERN_SUCCESS) {
    kern_return_t released = vm_deallocate(mach_task_self(), address, size);
    [self append:[NSString stringWithFormat:@"Liberación: %s (%d).\n", mach_error_string(released), released]];
  }
  [self append:@"La reserva fija anterior no valida el juego.\n"];

  // Let iPadOS choose the base instead of assuming the macOS 512 GiB address.
  // Only one backing page is touched; the 4 GiB allocation is virtual.
  vm_address_t dynamicBase = 0;
  result = vm_allocate(mach_task_self(), &dynamicBase, size, VM_FLAGS_ANYWHERE);
  [self append:[NSString stringWithFormat:@"Reserva dinámica 4 GiB: %s (%d), base 0x%llx.\n",
    mach_error_string(result), result, (unsigned long long)dynamicBase]];
  if (result != KERN_SUCCESS) {
    [self append:@"No se ejecutaron aliases dentro del espacio de 4 GiB.\n"];
    return;
  }
  result = vm_protect(mach_task_self(), dynamicBase, size, FALSE, VM_PROT_NONE);
  [self append:[NSString stringWithFormat:@"Protección de la reserva (sin acceso): %s (%d).\n",
    mach_error_string(result), result]];
  if (result != KERN_SUCCESS) {
    vm_deallocate(mach_task_self(), dynamicBase, size);
    return;
  }

  vm_address_t backing = 0;
  const vm_size_t pageSize = (vm_size_t)vm_page_size;
  result = vm_allocate(mach_task_self(), &backing, pageSize, VM_FLAGS_ANYWHERE);
  [self append:[NSString stringWithFormat:@"Backing de una página: %s (%d).\n",
    mach_error_string(result), result]];
  if (result == KERN_SUCCESS) {
    [self append:@"Iniciando aliases en offsets Wii 0x80000000 y 0xc0000000.\n"];
    vm_address_t alias1 = dynamicBase + 0x80000000ull;
    vm_address_t alias2 = dynamicBase + 0xc0000000ull;
    const vm_address_t wanted1 = alias1, wanted2 = alias2;
    vm_prot_t current1 = VM_PROT_NONE, maximum1 = VM_PROT_NONE;
    vm_prot_t current2 = VM_PROT_NONE, maximum2 = VM_PROT_NONE;
    // OVERWRITE is restricted to pages inside our own newly reserved region.
    kern_return_t remap1 = vm_remap(mach_task_self(), &alias1, pageSize, 0,
      VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(), backing,
      FALSE, &current1, &maximum1, VM_INHERIT_NONE);
    kern_return_t remap2 = vm_remap(mach_task_self(), &alias2, pageSize, 0,
      VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(), backing,
      FALSE, &current2, &maximum2, VM_INHERIT_NONE);
    [self append:[NSString stringWithFormat:@"vm_remap alias 1: %s (%d).\nvm_remap alias 2: %s (%d).\n",
      mach_error_string(remap1), remap1, mach_error_string(remap2), remap2]];
    const vm_prot_t needed = VM_PROT_READ | VM_PROT_WRITE;
    if (remap1 == KERN_SUCCESS && remap2 == KERN_SUCCESS &&
        alias1 == wanted1 && alias2 == wanted2 &&
        (current1 & needed) == needed && (current2 & needed) == needed) {
      [self append:@"Comprobando lecturas y escrituras compartidas de una página.\n"];
      auto *host = reinterpret_cast<volatile uint32_t*>(backing);
      auto *first = reinterpret_cast<volatile uint32_t*>(alias1);
      auto *second = reinterpret_cast<volatile uint32_t*>(alias2);
      *host = 0x13579bdfu;
      bool shared = *first == 0x13579bdfu && *second == 0x13579bdfu;
      *second = 0x2468ace0u;
      shared &= *host == 0x2468ace0u && *first == 0x2468ace0u;
      [self append:shared ? @"Aliases dinámicos: PASS (lectura y escritura compartidas).\n"
                          : @"Aliases dinámicos: FAIL (contenido no compartido).\n"];
    } else {
      [self append:@"Aliases dinámicos: FAIL o permisos incompatibles; no se accedió a las vistas.\n"];
    }
    kern_return_t freedBacking = vm_deallocate(mach_task_self(), backing, pageSize);
    [self append:[NSString stringWithFormat:@"Liberación backing: %s (%d).\n",
      mach_error_string(freedBacking), freedBacking]];
  }
  kern_return_t freedDynamic = vm_deallocate(mach_task_self(), dynamicBase, size);
  [self append:[NSString stringWithFormat:@"Liberación dinámica: %s (%d).\n",
    mach_error_string(freedDynamic), freedDynamic]];
  [self append:@"Esta prueba de una página no valida todas las regiones Wii ni el juego.\n"];
}
- (void)testContext {
  [self append:@"Iniciando cambios de contexto. Si la app se cierra, informar ese resultado.\n"];
  bool ok = TestContext();
  [self append:ok ? @"Contexto original: PASS (dos cambios con identidad preservada).\n"
                 : @"Contexto original: FAIL.\n"];
}
- (void)copyReport { UIPasteboard.generalPasteboard.string = self.report.text; }
- (void)drawInMTKView:(MTKView *)view {
  if (self.submitted || !self.queue) return;
  MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
  id<CAMetalDrawable> drawable = view.currentDrawable;
  if (!pass || !drawable) return;
  id<MTLCommandBuffer> command = [self.queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
  if (!command || !encoder) return;
  [encoder endEncoding];
  [command presentDrawable:drawable];
  self.submitted = YES;
  __weak ProbeController *weakSelf = self;
  [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
    BOOL ok = completed.status == MTLCommandBufferStatusCompleted;
    NSString *message = ok ? @"Metal: PASS (comando GPU completado; rectángulo azul).\n"
      : [NSString stringWithFormat:@"Metal: FAIL (%@).\n", completed.error];
    dispatch_async(dispatch_get_main_queue(), ^{ [weakSelf append:message]; });
  }];
  [command commit];
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
  self.submitted = NO;
  [view setNeedsDisplay];
}
@end

@interface ProbeDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end
@implementation ProbeDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)options {
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController = [[ProbeController alloc] init];
  [self.window makeKeyAndVisible];
  return YES;
}
@end
int main(int argc, char **argv) {
  @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(ProbeDelegate.class)); }
}
