#include "ProcessMonitor.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
    #include <tlhelp32.h>
    #include <psapi.h>
    #include <sddl.h>

    // Helper: Convert std::wstring to UTF-8 std::string.
    std::string wstringToUtf8(const std::wstring &wstr) {
        if (wstr.empty()) return std::string();
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
        return strTo;
    }

    // Helper: Get owner of the process.
    std::string getProcessOwner(HANDLE hProcess) {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) return "Unknown";
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(dwSize);
        if (!pTokenUser) { CloseHandle(hToken); return "Unknown"; }
        if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
            free(pTokenUser); CloseHandle(hToken); return "Unknown";
        }
        WCHAR szName[256], szDomain[256];
        DWORD dwNameSize = 256, dwDomainSize = 256;
        SID_NAME_USE snu;
        std::string owner = "Unknown";
        if (LookupAccountSidW(NULL, pTokenUser->User.Sid, szName, &dwNameSize, szDomain, &dwDomainSize, &snu)) {
            owner = wstringToUtf8(szName);
        }
        free(pTokenUser); CloseHandle(hToken);
        return owner;
    }

    Process getProcessInfo(int pid, const PROCESSENTRY32 &pe32) {
        Process proc;
        proc.pid = pid;
        proc.parentPid = static_cast<int>(pe32.th32ParentProcessID);
        proc.name = wstringToUtf8(pe32.szExeFile);
        proc.threadCount = pe32.cntThreads;
        proc.priority = 3;
        proc.status = "Running";
        proc.memoryMB = 0.0;
        proc.owner = "Unknown";

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess) {
            DWORD prClass = GetPriorityClass(hProcess);
            if      (prClass == IDLE_PRIORITY_CLASS)           proc.priority = 1;
            else if (prClass == BELOW_NORMAL_PRIORITY_CLASS)   proc.priority = 2;
            else if (prClass == NORMAL_PRIORITY_CLASS)         proc.priority = 3;
            else if (prClass == ABOVE_NORMAL_PRIORITY_CLASS)   proc.priority = 4;
            else if (prClass == HIGH_PRIORITY_CLASS)           proc.priority = 5;

            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                proc.memoryMB = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
            }
            proc.owner = getProcessOwner(hProcess);
            CloseHandle(hProcess);
        }
        return proc;
    }

#else
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/resource.h>
    #include <sys/stat.h>
    #include <pwd.h>
    #include <unistd.h>
#endif

Process getProcessInfo(int pid) {
    Process proc;
    proc.pid = pid;
    proc.parentPid = 0;
    proc.memoryMB = 0.0;
    proc.owner = "Unknown";
    proc.threadCount = 0;

#ifdef _WIN32
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (static_cast<int>(pe32.th32ProcessID) == pid) {
                    proc = getProcessInfo(pid, pe32);
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
#else
    std::string procPath = "/proc/" + std::to_string(pid);
    struct stat st;
    if (stat(procPath.c_str(), &st) == 0) {
        struct passwd *pw = getpwuid(st.st_uid);
        if (pw) proc.owner = std::string(pw->pw_name);
    }

    // Parse /proc/[pid]/status for memory
    std::ifstream statusFile(procPath + "/status");
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::stringstream ss(line.substr(6));
            long rss_kb; ss >> rss_kb;
            proc.memoryMB = (double)rss_kb / 1024.0;
        } else if (line.compare(0, 8, "Threads:") == 0) {
            std::stringstream ss(line.substr(8));
            ss >> proc.threadCount;
        }
    }

    // Parse /proc/[pid]/stat for name, status, and ppid
    std::ifstream statFile(procPath + "/stat");
    if (statFile.is_open()) {
        std::string statData;
        std::getline(statFile, statData);
        size_t start = statData.find('(');
        size_t end = statData.rfind(')');
        if (start != std::string::npos && end != std::string::npos) {
            proc.name = statData.substr(start + 1, end - start - 1);
            
            char stateChar;
            int ppid;
            if (sscanf(statData.c_str() + end + 1, " %c %d", &stateChar, &ppid) == 2) {
                proc.parentPid = ppid;
                switch (stateChar) {
                    case 'R': proc.status = "Running"; break;
                    case 'S': proc.status = "Sleeping"; break;
                    case 'D': proc.status = "Disk Sleep"; break;
                    case 'Z': proc.status = "Zombie"; break;
                    case 'T': proc.status = "Stopped"; break;
                    default: proc.status = "Unknown"; break;
                }
            }
        }
    }
    proc.priority = getpriority(PRIO_PROCESS, pid);
#endif
    return proc;
}

std::vector<Process> listProcesses() {
    std::vector<Process> processes;
#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return processes;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe32)) {
        do {
            processes.push_back(getProcessInfo(static_cast<int>(pe32.th32ProcessID), pe32));
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
#else
    DIR* dir = opendir("/proc");
    if (!dir) return processes;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string name(entry->d_name);
            if (std::all_of(name.begin(), name.end(), ::isdigit)) {
                processes.push_back(getProcessInfo(std::stoi(name)));
            }
        }
    }
    closedir(dir);
#endif
    return processes;
}

ProcessSnapshot::ProcessSnapshot() : isUpdating_(false) {}

void ProcessSnapshot::refresh() {
    bool expected = false;
    if (!isUpdating_.compare_exchange_strong(expected, true)) return;
    auto newProcesses = listProcesses();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_ = std::move(newProcesses);
    }
    isUpdating_ = false;
}

std::vector<Process> ProcessSnapshot::getProcesses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return processes_;
}

bool ProcessSnapshot::isUpdating() const {
    return isUpdating_.load();
}
