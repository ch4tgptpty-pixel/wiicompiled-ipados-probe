#pragma once
#include <string>
struct SDL_Window;
std::string ProbeDocumentsPath();
std::string ProbeCachePath();
void ProbeStatusAppend(const char* text);
void ProbeStatusAttach(SDL_Window* window);
