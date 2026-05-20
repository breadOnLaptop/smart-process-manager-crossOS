#ifndef SYSTEM_METRICS_H
#define SYSTEM_METRICS_H

#include <cstdint>

#ifdef _WIN32
    #include <windows.h>
#endif

class SystemMetrics {
public:
    SystemMetrics();
    virtual ~SystemMetrics() = default;

    // Returns CPU usage as a percentage (0.0 - 100.0)
    double getCPUUsage();

    // Returns RAM usage as a percentage (0.0 - 100.0)
    double getRAMUsage();

private:
#ifdef _WIN32
    FILETIME prev_idle_time_, prev_kernel_time_, prev_user_time_;
#else
    uint64_t prev_total_ = 0;
    uint64_t prev_idle_ = 0;
#endif
};

#endif // SYSTEM_METRICS_H
