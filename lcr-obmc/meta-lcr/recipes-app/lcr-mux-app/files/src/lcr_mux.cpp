#include "../include/lcr_mux.hpp"

CLI::CLI() {
    for (int i = 0; i < 4; i++) {
        mux_gpios.push_back(std::make_unique<gpio_info>(4, 3+i, 0));
    }
};

CLI::~CLI() {;};

int CLI::process_argument(int nargs, char* args[]) 
{
    if (nargs > 1) {
        std::string argument = args[1];
        int argint = std::stoi(argument);
        if (argint > 15) {
            std::cout << "Channel " << argint << " is too high. Must be a channel between 0 and 15.";
            return -1;
        } else if (argint < 0) {
            std::cout << "Channel " << argint << " is too low. Channels cannot be negative.";
            return -1;
        }

        std::cout << "Setting MUX channel to " << argint << std::endl;

        uint8_t channelselect = static_cast<uint8_t>(argint);

        for (int i = 0; i < 4; i++) {
            set_gpio(mux_gpios[i].get(), static_cast<int>((channelselect >> i) & 0x01));
        }
    }
    return 0;
};

int CLI::set_gpio(gpio_info* mux_gpio, int value)
{
    try {
        if (mux_gpio == nullptr) {
            std::cerr << "Error: GPIO is null" << std::endl;
            return -1;
        }
        if (value != 0 && value != 1) {
            std::cerr << "Error: Invalid value. Must be 0 or 1" << std::endl;
            return -1;
        }
    
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(mux_gpio->chip));
        
        // Get the line
        gpiod::line line = chip.get_line(mux_gpio->line);
        
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, value);
        
        // Update status
        mux_gpio->status = value;
        line.release();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

