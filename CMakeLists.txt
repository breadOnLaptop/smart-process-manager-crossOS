cmake_minimum_required(VERSION 3.10)
project(SPM)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW Setup
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

# ImGui Setup
set(IMGUI_DIR third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

include_directories(include)
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)
include_directories(third_party/glfw/include)

# Explicitly list source files now that we have more
set(SOURCES 
    src/main.cpp 
    src/WindowManager.cpp
    src/ProcessMonitor.cpp
    src/ProcessControl.cpp
    src/SystemMetrics.cpp
)

if(WIN32)
    add_executable(spm WIN32 ${SOURCES} ${IMGUI_SOURCES})
    # Ensure static linking for some runtimes if using MSVC to reduce DLL dependencies
    if(MSVC)
        set_target_properties(spm PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # Linker flag to use main() as entry point for WIN32 GUI application
        set_target_properties(spm PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
else()
    add_executable(spm ${SOURCES} ${IMGUI_SOURCES})
endif()

target_link_libraries(spm glfw)

if(WIN32)
    target_link_libraries(spm psapi opengl32 gdi32 imm32)
else()
    target_link_libraries(spm GL dl pthread)
endif()
