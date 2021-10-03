#ifndef APP_H_
#define APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class App {
public:
  bool Init();
  void Destroy();

  void MainLoop();

private:
  GLFWwindow* window_;

  VkInstance instance_;
};

#endif // APP_H_