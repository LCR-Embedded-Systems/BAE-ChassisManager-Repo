#include "../include/global.hpp"

namespace fs = std::filesystem;

std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string timeSinceBoot()
{
    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return "Error: Unable to read /proc/uptime";
    }

    double uptime_seconds;
    uptime_file >> uptime_seconds;
    uptime_file.close();

    // Convert to days, hours, minutes, seconds
    int days = static_cast<int>(uptime_seconds) / 86400;
    int hours = (static_cast<int>(uptime_seconds) % 86400) / 3600;
    int minutes = (static_cast<int>(uptime_seconds) % 3600) / 60;
    int seconds = static_cast<int>(uptime_seconds) % 60;

    std::ostringstream oss;
    if (days > 0) {
        oss << days << "d ";
    }
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds;
    
    return oss.str();
}

std::string readFile(std::string path) {
    std::string val;
    std::ifstream readingFile(path);
    readingFile >> val;
    return val;
}

