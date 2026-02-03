#include "../include/memory.hpp"

int main(int argc, char* argv[]) {

    if (argc > 1) {
        std::string command = argv[1];
        if (command == "save") {
            std::cout << "Running save" << std::endl;
            run_save_vars();
        } else if (command == "load") {
            std::cout << "Running load" << std::endl;
            run_read_vars();
        } else if (command == "default") {
            std::cout << "Running default" << std::endl;
            run_set_defaults();
        } else {
            std::cout << "Unknown command" << std::endl;
        }
    }
}