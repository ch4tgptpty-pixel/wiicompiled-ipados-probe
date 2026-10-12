#pragma once
#include <functional>
#include <string>
bool RunIosMemoryTests(std::function<void(const std::string&)> report);
