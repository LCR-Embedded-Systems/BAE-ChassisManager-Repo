
#include "../include/lcr_interface.hpp"

int main(int argc, char* argv[])
{

    CLI cli;

    if (argc > 1) {
        cli.process_argument(argc-1, argv);
    } else {
        std::cout << "Available commands are:" << std::endl;
        std::cout << "\ttemperature: display all temperatures and threshold statuses" << std::endl;
        std::cout << "\tfans: display all fans and threshold statuses" << std::endl;
        std::cout << "\tvoltage: display all voltage readings and threshold statuses" << std::endl;
        std::cout << "\talarms: display all alarm statuses and their thresholds" << std::endl;
        std::cout << "\tall: display all available information" << std::endl;
        std::cout << "\tset: set the thresholds for the alarms" << std::endl;
    }

    return 0;
}
