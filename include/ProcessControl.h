#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

// Kills the process with the given PID using a tiered strategy.
bool killProcess(int pid);

// Changes the process's priority.
bool changeProcessPriority(int pid, int newPriority);

#endif // PROCESS_CONTROL_H
