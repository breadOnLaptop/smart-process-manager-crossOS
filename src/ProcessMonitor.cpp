#include "ProcessMonitor.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

namespace {
    std::string wstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    std::string getProcessOwner(HANDLE hProcess) {
        HANDLE hToken;
        if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) return "Unknown";
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
        PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(dwSize);
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

    Process getProcessInfo(int pid, const PROCESSENTRY32W &pe32) {
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
            else if (prClass == HIGH_PRIORITY_CLASS)          proc.priority = 5;
            else if (prClass == REALTIME_PRIORITY_CLASS)      proc.priority = 6;

            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                proc.memoryMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
            }
            proc.owner = getProcessOwner(hProcess);
            CloseHandle(hProcess);
        }
        return proc;
    }
}

void ProcessSnapshot::refresh() {
    std::vector<Process> newProcesses;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                newProcesses.push_back(getProcessInfo(pe32.th32ProcessID, pe32));
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    std::lock_guard<std::mutex> lock(mtx);
    processes = std::move(newProcesses);
}

#else
// Linux implementation
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

void ProcessSnapshot::refresh() {
    std::vector<Process> newProcesses;
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (isdigit(entry->d_name[0])) {
                int pid = atoi(entry->d_name);
                std::string path = "/proc/" + std::string(entry->d_name);
                
                // Name & Parent PID & Threads from /proc/[pid]/stat
                std::ifstream statFile(path + "/stat");
                std::string line;
                if (std::getline(statFile, line)) {
                    size_t firstParen = line.find('(');
                    size_t lastParen = line.rfind(')');
                    if (firstParen != std::string::npos && lastParen != std::string::npos) {
                        Process proc;
                        proc.pid = pid;
                        proc.name = line.substr(firstParen + 1, lastParen - firstParen - 1);
                        
                        std::string rest = line.substr(lastParen + 2);
                        std::stringstream ss(rest);
                        std::string state;
                        int ppid, pgrp, session, tty_nr, tpgid;
                        unsigned int flags;
                        unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;
                        long cutime, cstime, priority, nice, num_threads;
                        
                        ss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags 
                           >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime 
                           >> cutime >> cstime >> priority >> nice >> num_threads;

                        proc.status = (state == "R" ? "Running" : (state == "S" ? "Sleeping" : "Other"));
                        proc.parentPid = ppid;
                        proc.threadCount = static_cast<int>(num_threads);
                        proc.priority = static_cast<int>(nice);

                        // Memory from /proc/[pid]/status
                        std::ifstream statusFile(path + "/status");
                        std::string statusLine;
                        proc.memoryMB = 0;
                        while (std::getline(statusFile, statusLine)) {
                            if (statusLine.find("VmRSS:") == 0) {
                                std::stringstream ssMem(statusLine.substr(6));
                                long kb;
                                ssMem >> kb;
                                proc.memoryMB = kb / 1024.0;
                                break;
                            }
                        }

                        // Owner
                        struct stat st;
                        if (stat(path.c_str(), &st) == 0) {
                            struct passwd* pw = getpwuid(st.st_uid);
                            proc.owner = pw ? pw->pw_name : std::to_string(st.st_uid);
                        }

                        newProcesses.push_back(proc);
                    }
                }
            }
        }
        closedir(dir);
    }
    std::lock_guard<std::mutex> lock(mtx);
    processes = std::move(newProcesses);
}
#endif

std::vector<Process> ProcessSnapshot::getProcesses() {
    std::lock_guard<std::mutex> lock(mtx);
    return processes;
}
