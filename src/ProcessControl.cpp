#include "ProcessControl.h"
#include <chrono>
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <signal.h>
    #include <sys/resource.h>
    #include <unistd.h>
#endif

#ifdef _WIN32
struct TerminateData {
    DWORD dwProcessId;
    bool bSentClose;
};

BOOL CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam) {
    TerminateData* pData = reinterpret_cast<TerminateData*>(lParam);
    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hwnd, &dwProcessId);
    if (dwProcessId == pData->dwProcessId) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        pData->bSentClose = true;
    }
    return TRUE;
}
#endif

bool killProcess(int pid) {
#ifdef _WIN32
    TerminateData data = { (DWORD)pid, false };
    EnumWindows(TerminateAppEnum, reinterpret_cast<LPARAM>(&data));
    if (data.bSentClose) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) return true; 
    DWORD dwExitCode = 0;
    if (GetExitCodeProcess(hProcess, &dwExitCode) && dwExitCode != STILL_ACTIVE) {
        CloseHandle(hProcess);
        return true;
    }
    bool result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result;
#else
    if (kill(pid, SIGTERM) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (kill(pid, 0) != 0) return true;
    }
    return (kill(pid, SIGKILL) == 0);
#endif
}

bool changeProcessPriority(int pid, int newPriority) {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    int priorityClass;
    switch (newPriority) {
        case 1: priorityClass = IDLE_PRIORITY_CLASS; break;
        case 2: priorityClass = BELOW_NORMAL_PRIORITY_CLASS; break;
        case 3: priorityClass = NORMAL_PRIORITY_CLASS; break;
        case 4: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
        case 5: priorityClass = HIGH_PRIORITY_CLASS; break;
        default: priorityClass = NORMAL_PRIORITY_CLASS;
    }
    bool success = SetPriorityClass(hProcess, priorityClass);
    CloseHandle(hProcess);
    return success;
#else
    return (setpriority(PRIO_PROCESS, pid, newPriority) == 0);
#endif
}
