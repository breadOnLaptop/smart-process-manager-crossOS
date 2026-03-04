#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include "../include/ProcessMonitor.h"
#include "../include/ProcessControl.h"
#include "../include/Logger.h"
#include "../include/WindowManager.h"
#include "../include/SystemMetrics.h"
#include "imgui.h"

using namespace std;

// Global state
ProcessSnapshot g_Snapshot;
SystemMetrics g_Metrics;
atomic<bool> g_Running(true);
atomic<int> g_RefreshIntervalMs(2000);

// Corrected contiguous Rolling Buffer for ImGui
struct RollingBuffer {
    vector<float> data;
    int maxSize;
    RollingBuffer(int size) : maxSize(size) {
        data.resize(size, 0.0f);
    }
    void add(float val) {
        // Shift left and add to end to keep it contiguous for PlotLines
        for (int i = 0; i < maxSize - 1; i++) {
            data[i] = data[i + 1];
        }
        data[maxSize - 1] = val;
    }
    const float* getData() const { return data.data(); }
};

// Background thread for automatic refresh.
void backgroundRefresh() {
    while (g_Running) {
        int interval = g_RefreshIntervalMs.load();
        if (interval > 0) {
            g_Snapshot.refresh();
            this_thread::sleep_for(chrono::milliseconds(interval));
        } else {
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }
}

// Helper: Check if process belongs to system/root.
bool isSystemProcess(const string& owner) {
    string o = owner;
    transform(o.begin(), o.end(), o.begin(), ::tolower);
    return (o.find("system") != string::npos || 
            o.find("service") != string::npos || 
            o.find("root") != string::npos);
}

// Helper: Draw a Task-Manager-like grid
void DrawGraphGrid(ImVec2 pos, ImVec2 size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 gridColor = ImColor(255, 255, 255, 30); // Faint white
    
    // Vertical lines
    for (int i = 0; i <= 10; i++) {
        float x = pos.x + (size.x / 10.0f) * i;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + size.y), gridColor);
    }
    // Horizontal lines
    for (int i = 0; i <= 5; i++) {
        float y = pos.y + (size.y / 5.0f) * i;
        drawList->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), gridColor);
    }
}

int main() {
    ImplementationLogger::init("logs/implementation_log.txt");
    ProcessLogger::init("logs/process_log.txt");
    ImplementationLogger::log(ImplementationLogger::INFO, "SPM GUI started.");

    g_Snapshot.refresh();
    thread refreshThread(backgroundRefresh);

    WindowManager windowManager;
    if (!windowManager.init(1280, 720, "Smart Process Manager")) {
        g_Running = false;
        refreshThread.join();
        return 1;
    }

    static char searchFilter[128] = "";
    static int selectedPid = -1;
    static string selectedOwner = "";
    static int refreshChoice = 2;

    RollingBuffer cpuBuffer(60);
    RollingBuffer ramBuffer(60);
    float lastMetricUpdate = 0;

    // Main loop
    while (!windowManager.shouldClose()) {
        windowManager.startFrame();

        float currentTime = (float)ImGui::GetTime();
        if (currentTime - lastMetricUpdate >= 1.0f) {
            cpuBuffer.add((float)g_Metrics.getCPUUsage());
            ramBuffer.add((float)g_Metrics.getRAMUsage());
            lastMetricUpdate = currentTime;
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (ImGui::BeginTabBar("MainTabs")) {
            
            if (ImGui::BeginTabItem("Processes")) {
                ImGui::Columns(2, "MainColumns", true);
                ImGui::SetColumnWidth(0, ImGui::GetIO().DisplaySize.x * 0.75f);

                ImGui::Text("Smart Process Manager (SPM)");
                ImGui::SameLine(ImGui::GetColumnWidth(0) - 220);
                const char* refreshRates[] = { "Paused", "500ms", "1s", "2s", "5s" };
                int msValues[] = { 0, 500, 1000, 2000, 5000 };
                ImGui::PushItemWidth(120);
                if (ImGui::Combo("Refresh", &refreshChoice, refreshRates, IM_ARRAYSIZE(refreshRates))) {
                    g_RefreshIntervalMs.store(msValues[refreshChoice]);
                }
                ImGui::PopItemWidth();
                ImGui::Separator();
                ImGui::PushItemWidth(-1);
                ImGui::InputTextWithHint("##Search", "Search processes...", searchFilter, IM_ARRAYSIZE(searchFilter));
                ImGui::PopItemWidth();

                auto processes = g_Snapshot.getProcesses();
                string filterStr = string(searchFilter);
                transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

                if (ImGui::BeginTable("ProcessTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0, -ImGui::GetFrameHeightWithSpacing()))) {
                    ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Mem (MB)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                    ImGui::TableHeadersRow();

                    for (const auto& proc : processes) {
                        string nameLower = proc.name;
                        string ownerLower = proc.owner;
                        transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        transform(ownerLower.begin(), ownerLower.end(), ownerLower.begin(), ::tolower);
                        if (!filterStr.empty() && nameLower.find(filterStr) == string::npos && ownerLower.find(filterStr) == string::npos)
                            continue;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        bool isSelected = (selectedPid == proc.pid);
                        if (ImGui::Selectable(to_string(proc.pid).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                            selectedPid = proc.pid;
                            selectedOwner = proc.owner;
                        }
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", proc.name.c_str());
                        ImGui::TableSetColumnIndex(2); ImGui::Text("%s", proc.status.c_str());
                        ImGui::TableSetColumnIndex(3); ImGui::Text("%d", proc.priority);
                        ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", proc.memoryMB);
                        ImGui::TableSetColumnIndex(5); ImGui::Text("%s", proc.owner.c_str());
                    }
                    ImGui::EndTable();
                }

                ImGui::NextColumn();
                ImGui::Text("Actions & Details");
                ImGui::Separator();
                if (selectedPid != -1) {
                    ImGui::Text("PID: %d", selectedPid);
                    ImGui::Text("Owner: %s", selectedOwner.c_str());
                    bool isProtected = isSystemProcess(selectedOwner);
                    if (isProtected) { ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "System Protected"); ImGui::BeginDisabled(); }
                    if (ImGui::Button("Terminate", ImVec2(-FLT_MIN, 40))) { if (killProcess(selectedPid)) { selectedPid = -1; g_Snapshot.refresh(); } }
                    static int priorityLevel = 2;
                    ImGui::Combo("Priority", &priorityLevel, "Idle\0Below Normal\0Normal\0Above Normal\0High\0");
                    if (ImGui::Button("Apply", ImVec2(-FLT_MIN, 30))) { changeProcessPriority(selectedPid, priorityLevel + 1); }
                    if (isProtected) ImGui::EndDisabled();
                } else { ImGui::TextDisabled("Select a process..."); }
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance")) {
                ImGui::Text("Global System Performance");
                ImGui::Separator();

                ImVec2 graphSize = ImVec2(-FLT_MIN, 180);
                
                // CPU Graph
                ImGui::Text("CPU Usage: %.1f%%", cpuBuffer.data.back());
                ImGui::ProgressBar(cpuBuffer.data.back() / 100.0f, ImVec2(-FLT_MIN, 0));
                
                ImVec2 cpuPos = ImGui::GetCursorScreenPos();
                DrawGraphGrid(cpuPos, graphSize);
                ImGui::PlotLines("##CPUGraph", cpuBuffer.getData(), cpuBuffer.maxSize, 0, nullptr, 0.0f, 100.0f, graphSize);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // RAM Graph
                ImGui::Text("RAM Usage: %.1f%%", ramBuffer.data.back());
                ImGui::ProgressBar(ramBuffer.data.back() / 100.0f, ImVec2(-FLT_MIN, 0));
                
                ImVec2 ramPos = ImGui::GetCursorScreenPos();
                DrawGraphGrid(ramPos, graphSize);
                ImGui::PlotLines("##RAMGraph", ramBuffer.getData(), ramBuffer.maxSize, 0, nullptr, 0.0f, 100.0f, graphSize);

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
        windowManager.endFrame();
    }

    g_Running = false;
    refreshThread.join();
    return 0;
}
