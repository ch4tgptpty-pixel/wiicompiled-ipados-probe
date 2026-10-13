#include "ios_memory_tests.h"
#include <iostream>
bool RunIosMemoryAccessTests(std::function<void(const std::string&)> report);
int main() {
    auto report=[](const std::string& s){ std::cout<<s<<std::endl; };
    return RunIosMemoryTests(report) && RunIosMemoryAccessTests(report) ? 0 : 1;
}
