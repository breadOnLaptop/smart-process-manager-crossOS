#include "WindowManager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

WindowManager::WindowManager() : window_(nullptr) {
    #ifdef __APPLE__
        glsl_version_ = "#version 150";
    #else
        glsl_version_ = "#version 130";
    #endif
}

WindowManager::~WindowManager() {
    shutdown();
}

bool WindowManager::init(int width, int height, const std::string& title) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return false;

    // Create window with graphics context
    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window_ == nullptr) return false;
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(10, 8);
    
    // Vibrant Blue Accents
    style.Colors[ImGuiCol_Header]         = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]   = ImVec4(0.25f, 0.60f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.10f, 0.35f, 0.65f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.15f, 0.45f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]   = ImVec4(0.20f, 0.55f, 0.85f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]      = ImVec4(0.30f, 0.65f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]     = ImVec4(0.30f, 0.65f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.75f, 1.00f, 1.00f);

    // Load Fonts
    #ifdef _WIN32
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f)) {
        std::cout << "Loaded Segoe UI font." << std::endl;
    } else {
        io.Fonts->AddFontDefault();
    }
    #else
    io.Fonts->AddFontDefault();
    #endif

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version_.c_str());

    return true;
}

bool WindowManager::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void WindowManager::startFrame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void WindowManager::endFrame() {
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void WindowManager::shutdown() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}
