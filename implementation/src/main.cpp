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
#include "imgui.h"

using namespace std;

// Global state
ProcessSnapshot g_Snapshot;
atomic<bool> g_Running(true);
atomic<int> g_RefreshIntervalMs(2000); // Default 2s

// Background thread for automatic refresh.
void backgroundRefresh() {
    while (g_Running) {
        int interval = g_RefreshIntervalMs.load();
        if (interval > 0) {
            g_Snapshot.refresh();
            this_thread::sleep_for(chrono::milliseconds(interval));
        } else {
            // Paused: Sleep a bit to avoid busy-waiting.
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

int main() {
    // Initialize loggers.
    ImplementationLogger::init("logs/implementation_log.txt");
    ProcessLogger::init("logs/process_log.txt");
    ImplementationLogger::log(ImplementationLogger::INFO, "SPM GUI started.");

    // Initial refresh and start background thread.
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
    static int refreshChoice = 2; // Index for 2s

    // Main loop
    while (!windowManager.shouldClose()) {
        windowManager.startFrame();

        // UI Code
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, ImGui::GetIO().DisplaySize.x * 0.75f);

        // --- LEFT COLUMN: Process List ---
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
        if (ImGui::InputTextWithHint("##Search", "Search processes (e.g. chrome, explorer)...", searchFilter, IM_ARRAYSIZE(searchFilter))) {
            // Optional: reset selection on search change to avoid confusion.
        }
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

            bool foundSelection = false;
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
                if (isSelected) foundSelection = true;
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", proc.name.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", proc.status.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", proc.priority);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.2f", proc.memoryMB);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%s", proc.owner.c_str());
            }
            // If selection is filtered out, we keep it but the user can't see it.
            // Usually fine, but if we want to clear it:
            // if (!foundSelection) selectedPid = -1;

            ImGui::EndTable();
        }

        ImGui::NextColumn();

        // --- RIGHT COLUMN: Actions ---
        ImGui::Text("Actions & Details");
        ImGui::Separator();

        if (selectedPid != -1) {
            ImGui::Text("PID: %d", selectedPid);
            ImGui::Text("Owner: %s", selectedOwner.c_str());
            
            bool isProtected = isSystemProcess(selectedOwner);
            
            if (isProtected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("System Protection: Actions disabled for security.");
                ImGui::PopStyleColor();
                ImGui::BeginDisabled();
            }

            ImGui::Spacing();
            if (ImGui::Button("Terminate Process", ImVec2(-FLT_MIN, 40))) {
                if (killProcess(selectedPid)) {
                    ImplementationLogger::log(ImplementationLogger::INFO, "GUI: Terminated PID " + to_string(selectedPid));
                    selectedPid = -1;
                    g_Snapshot.refresh();
                }
            }

            ImGui::Separator();
            ImGui::Text("Priority Class:");
            static int priorityLevel = 2; // Normal
            const char* levels[] = { "Idle", "Below Normal", "Normal", "Above Normal", "High" };
            ImGui::Combo("##PriorityCombo", &priorityLevel, levels, IM_ARRAYSIZE(levels));
            
            if (ImGui::Button("Apply Priority", ImVec2(-FLT_MIN, 30))) {
                if (changeProcessPriority(selectedPid, priorityLevel + 1)) {
                    ImplementationLogger::log(ImplementationLogger::INFO, "GUI: Changed Priority for PID " + to_string(selectedPid));
                }
            }

            if (isProtected) {
                ImGui::EndDisabled();
            }
        } else {
            ImGui::TextDisabled("Select a process from the table\nto view details and actions.");
        }

        ImGui::End();

        windowManager.endFrame();
    }

    g_Running = false;
    refreshThread.join();
    ImplementationLogger::log(ImplementationLogger::INFO, "Exiting SPM GUI.");
    return 0;
}
