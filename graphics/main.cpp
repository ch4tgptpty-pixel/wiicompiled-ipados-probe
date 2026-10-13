#include <aurora/aurora.h>
int main(int argc, char** argv) {
  AuroraConfig config{};
  config.appName = "Graphics dependency probe";
  auto info = aurora_initialize(argc, argv, &config);
  return info.initializationStatus == AURORA_INITIALIZATION_SUCCESS ? 0 : 1;
}
