#include "global.hpp"

#include <fstream>
#include <ostream>
#include <cstdint>
#include <string>

#include <systemd/sd-bus.h>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

struct gpio_info {
    int chip;
    int line;
    int status;
    gpio_info(int chipin, int linein, int statusin) : chip(chipin), line(linein), status(statusin) {}
};

class CLI
{

    public:
    
        CLI();
        ~CLI();

        int process_argument(int nargs, char* args[]);

    private:

        std::vector<std::unique_ptr<gpio_info>> mux_gpios;

        int set_gpio(gpio_info* mux_gpio, int value);

};
