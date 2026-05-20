#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <vector>
#include <string>
#include <mutex>
#include <atomic>

// Structure representing a process.
struct Process {
    int pid;
    int parentPid;
    std::string name;
    std::string status;
    int priority;
    double memoryMB;
    std::string owner;
    int threadCount;
};

// Lists all processes on the system.
std::vector<Process> listProcesses();

// Gets detailed info for a single process.
Process getProcessInfo(int pid);

// Class to manage thread-safe process snapshots.
class ProcessSnapshot {
public:
    ProcessSnapshot();
    void refresh();
    std::vector<Process> getProcesses() const;
    bool isUpdating() const;

private:
    std::vector<Process> processes_;
    mutable std::mutex mutex_;
    std::atomic<bool> isUpdating_;
};

#endif // PROCESS_MONITOR_H
