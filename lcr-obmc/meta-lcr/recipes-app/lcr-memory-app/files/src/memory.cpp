#include "../include/memory.hpp"

void run_save_vars() {

    system("flashcp -v /usr/share/thresholds/thresholds.json /dev/mtd3");
}

void run_read_vars() {

    std::ifstream file("/usr/share/thresholds/thresholds.json");
    int lines_to_read;
    std::string line;
    while (std::getline(file, line)) {
        ++lines_to_read;
    }
    file.close();
    
    std::string filename = "/dev/mtd3";
    std::ifstream fileo(filename);
    
    std::ofstream outFile("/usr/share/thresholds/thresholds.json");
    std::string lineo;
    int line_count = 0;

    while (std::getline(fileo, lineo) && line_count < (lines_to_read - 2) ) {
        // Write line to output file with a newline
        size_t lineLength = lineo.length(); // Get the length of the current line
        if (lineLength < 32 && lineo[0] != '}') {
            outFile << lineo << '\n';
        } else {
            break;
        }
        line_count++;
        
    }
    outFile << '}';
    outFile << '\n';
    outFile.close();
}

void run_set_defaults() {
    std::cout << "Set defaults, erasing thresholds and replacing them with firmware defaults..." << std::endl;

    system("flashcp -v /usr/share/thresholds/thresholds_default.json /dev/mtd3");
    run_read_vars();

}
