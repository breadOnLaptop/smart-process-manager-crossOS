#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include "../include/ProcessMonitor.h"
#include "../include/ProcessControl.h"
#include "../include/Logger.h"
#include "../include/Formatter.h"

using namespace std;

// Robust integer input helper.
int getSafeInt(const string& prompt = "Enter choice: ") {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        } else {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "[ERROR] Invalid input. Please enter a valid number." << endl;
        }
    }
}

int main() {
    int choice;
    int pid, newPriority;
    
    // Initialize loggers.
    ImplementationLogger::init("logs/implementation_log.txt");
    ProcessLogger::init("logs/process_log.txt");
    ImplementationLogger::log(ImplementationLogger::INFO, "SPM started.");
    
    while (true) {
        cout << "\n=== Smart Process Manager (SPM) ===" << endl;
        cout << "1. List Processes" << endl;
        cout << "2. Kill Process" << endl;
        cout << "3. Change Process Priority" << endl;
        cout << "4. Update Process Log" << endl;
        cout << "5. Exit" << endl;
        
        choice = getSafeInt("Enter your choice: ");
        
        switch (choice) {
            case 1: {
                ImplementationLogger::log(ImplementationLogger::INFO, "Listing processes.");
                auto processes = listProcesses();
                cout << Formatter::formatProcessTable(processes);
                break;
            }
            case 2: {
                pid = getSafeInt("Enter PID to kill: ");
                if (killProcess(pid)) {
                    cout << "Process " << pid << " termination signal sent." << endl;
                    ImplementationLogger::log(ImplementationLogger::INFO, "Killed process with PID " + to_string(pid));
                } else {
                    cout << "[ERROR] Failed to kill process " << pid << ". Ensure you have sufficient permissions." << endl;
                    ImplementationLogger::log(ImplementationLogger::LOG_ERROR, "Failed to kill process with PID " + to_string(pid));
                }
                break;
            }
            case 3: {
                pid = getSafeInt("Enter PID to change priority: ");
                
                auto processes = listProcesses();
                auto it = std::find_if(processes.begin(), processes.end(), [pid](const Process &p) {
                    return p.pid == pid;
                });
                if (it == processes.end()) {
                    cout << "[ERROR] Process ID " << pid << " not found. Please enter a valid PID." << endl;
                    ImplementationLogger::log(ImplementationLogger::LOG_ERROR, "Attempted to change priority for invalid PID " + to_string(pid));
                    break;
                }
                
                Process beforeChange = getProcessInfo(pid);
                cout << "Current priority for process " << pid << ": " << beforeChange.priority << endl;
                newPriority = getSafeInt("Enter desired priority (Win: 1-5, Linux: -20 to 19): ");

                if (changeProcessPriority(pid, newPriority)) {
                    cout << "Priority change requested for process " << pid << "." << endl;
                    ImplementationLogger::log(ImplementationLogger::INFO, "Changed priority for PID " + to_string(pid));
                } else {
                    cout << "[ERROR] Failed to change priority for process " << pid << "." << endl;
                    ImplementationLogger::log(ImplementationLogger::LOG_ERROR, "Failed to change priority for PID " + to_string(pid));
                }
                break;
            }
            case 4: {
                auto processes = listProcesses();
                ProcessLogger::update(processes);
                cout << "Process log updated (table format logged to file)." << endl;
                break;
            }
            case 5: {
                ImplementationLogger::log(ImplementationLogger::INFO, "Exiting SPM.");
                goto exit_loop;
            }
            default: {
                cout << "Invalid choice! Try again." << endl;
                ImplementationLogger::log(ImplementationLogger::DEBUG, "User entered an invalid menu choice.");
                break;
            }
        }
    }
exit_loop:
    ImplementationLogger::close();
    ProcessLogger::close();
    return 0;
}
