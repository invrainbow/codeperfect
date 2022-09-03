#include "glcrap.hpp"
#include "ostype.hpp"
#include "wintype.hpp"

#if WIN_GLFW

#if OS_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif OS_WINBLOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3native.h>

void* get_native_window_handle(GLFWwindow *window) {
#if OS_LINUX
    return (void*)glfwGetX11Window(window);
#elif OS_WINBLOWS
    return (void*)glfwGetWin32Window(window);
#endif
}

#endif // WIN_GLFW
