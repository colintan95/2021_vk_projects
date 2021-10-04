#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "app.h"

int main() {
  App app;
  if (!app.Init()) {
    std::cerr << "Failed to initialize." << std::endl;
    return -1;
  }
  app.MainLoop();
  app.Destroy();

  return 0;
}