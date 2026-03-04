#ifndef FORMATTER_H
#define FORMATTER_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "ProcessMonitor.h"

class Formatter {
public:
    // Helper to center align a string.
    static std::string center(const std::string &s, int width) {
        int len = s.length();
        if (width <= len) return s;
        int pad = width - len;
        int padLeft = pad / 2;
        int padRight = pad - padLeft;
        return std::string(padLeft, ' ') + s + std::string(padRight, ' ');
    }

    // Generates a horizontal border line for the table.
    static std::string horizontalLine(const std::vector<int>& widths) {
        std::string line = "+";
        for (int w : widths) {
            line += std::string(w, '-') + "+";
        }
        return line + "\n";
    }

    // Formats a list of processes into a CLI-friendly table string.
    static std::string formatProcessTable(const std::vector<Process>& processes) {
        std::vector<int> widths = {7, 20, 15, 8, 12, 15}; // PID, Name, Status, Priority, Mem, Owner
        std::ostringstream oss;

        oss << horizontalLine(widths);
        oss << "|" << center("PID", widths[0])
            << "|" << center("Name", widths[1])
            << "|" << center("Status", widths[2])
            << "|" << center("Priority", widths[3])
            << "|" << center("Mem (MB)", widths[4])
            << "|" << center("Owner", widths[5])
            << "|\n";
        oss << horizontalLine(widths);

        for (const auto& proc : processes) {
            std::string displayName = proc.name;
            if (displayName.length() > widths[1]) {
                displayName = displayName.substr(0, widths[1] - 3) + "...";
            }

            std::stringstream memStream;
            memStream << std::fixed << std::setprecision(2) << proc.memoryMB;

            oss << "|" << center(std::to_string(proc.pid), widths[0])
                << "|" << center(displayName, widths[1])
                << "|" << center(proc.status, widths[2])
                << "|" << center(std::to_string(proc.priority), widths[3])
                << "|" << center(memStream.str(), widths[4])
                << "|" << center(proc.owner, widths[5])
                << "|\n";
        }
        oss << horizontalLine(widths);
        return oss.str();
    }
};

#endif // FORMATTER_H
