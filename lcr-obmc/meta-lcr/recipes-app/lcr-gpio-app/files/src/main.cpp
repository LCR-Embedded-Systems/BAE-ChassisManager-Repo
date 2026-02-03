
#include "../include/lcr_gpio.hpp"

int main(int argc, char* argv[])
{

    CLI cli;
    if (argc > 1) {
        cli.process_argument(argc, argv);
    } else {
        std::cout << "No arguments provided. Options displayed below:" << std::endl;
        cli.display_options();
    }

    return 0;
}
