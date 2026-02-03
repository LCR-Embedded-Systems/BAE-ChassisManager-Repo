#include "../include/global.hpp"

namespace fs = std::filesystem;

bool isValidDirectory(const std::string& directory) {
    // std::string path = "/sys/class/hwmon/" + directory + "/";
    if (!fs::is_directory(directory)) {
        return false;
    };
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();

            if (entry.path().filename() == "name") {
                std::ifstream file(filePath);
                if (file.is_open()) {
                    std::string content;
                    std::getline(file, content);  // read one line
                
                    // remove any trailing '\r' or '\n' just in case
                    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
                    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
                    //dbgFile << "[" << currentTimestamp() << "] " << "LCR:TEMP: Content of name file: " << content << std::endl;
                    if (content == "tmp100") {
                        return true;
                    }
                } else {
                    ;
                }
            }
        }
    }
    return false;
}

std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string readFile(std::string path) {
    std::string val;
    std::ifstream readingFile(path);
    readingFile >> val;
    return val;
}

