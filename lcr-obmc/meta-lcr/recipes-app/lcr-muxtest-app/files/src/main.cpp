#include "../include/lcr_muxtest.hpp"

int main(int argc, char* argv[])
{

    muxtest inst;
    
    std::cout << "Mux test initiated..." << std::endl;
    std::cout << "Ensure that all UARTs are tied to a loop back." << std::endl;
    std::cout << "This test will send serial down the tx, then read the rx and ensure the data sent is also received." << std::endl;
    std::cout << "Once all of the UARTs are looped back, press Enter to continue...";
    std::cin.get();

    if (inst.runtest()) {
        std::cout << "Test failed... ensure that all UARTs are properly looped back and try again." << std::endl;
    } else {
        std::cout << "Test success! Press enter to exit test...";
        std::cin.get();
    }

    return 0;
}
