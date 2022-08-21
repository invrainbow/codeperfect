#include "glcrap.hpp"

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

void* get_linux_native_window_handle(GLFWwindow *window) {
    return (void*)glfwGetX11Window(window);
}
