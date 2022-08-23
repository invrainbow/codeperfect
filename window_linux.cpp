#include "glcrap.hpp"
#include "ostype.hpp"

#if OS_LINUX

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

void* get_native_window_handle(GLFWwindow *window) {
    return (void*)glfwGetX11Window(window);
}

#endif // OS_LINUX
