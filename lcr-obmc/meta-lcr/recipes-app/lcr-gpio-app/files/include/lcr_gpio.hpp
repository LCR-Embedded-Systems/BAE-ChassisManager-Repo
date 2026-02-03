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

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;

struct gpio_info {
    bool input;
    int chip;
    int line;
    int status;
    std::string name;
};

class CLI
{

    public:
    
        CLI();
        ~CLI();

        int process_argument(int nargs, char* args[]);

        void display_options();

        void display_PINs();

        void readallpins();

    private:

        sdbusplus::bus::bus bus;

        std::vector<gpio_info> available_gpios;
        std::vector<gpio_info> chassis_gpios;

        gpio_info sysreset;
        gpio_info nvmro;
        gpio_info gdiscrete;
        gpio_info psenable;
        gpio_info ps1inh;
        gpio_info ps1fail;
        gpio_info ps2inh;
        gpio_info ps2fail;

        gpio_info BP_GPIO_OUT0;
        gpio_info BP_GPIO_OUT1;
        gpio_info BP_GPIO_OUT2;
        gpio_info BP_GPIO_OUT3;
        gpio_info BP_GPIO_OUT4;
        gpio_info BP_GPIO_OUT5;
        gpio_info BP_GPIO_OUT6;
        gpio_info BP_GPIO_OUT7;

        gpio_info BP_GPIO_IN0;
        gpio_info BP_GPIO_IN1;
        gpio_info BP_GPIO_IN2;
        gpio_info BP_GPIO_IN3;
        gpio_info BP_GPIO_IN4;
        gpio_info BP_GPIO_IN5;
        gpio_info BP_GPIO_IN6;
        gpio_info BP_GPIO_IN7;

        void build_gpio_list();
        gpio_info populate_info(bool input, int chip, int line, int status, std::string name);

        gpio_info* get_gpio_by_name(const std::string& name);

        gpio_info* get_chassis_gpio_by_name(const std::string& name);
        
        int setgpio(gpio_info* target_gpio, int value);

        int setdbusgpio(gpio_info* target_gpio, int value);

        int readgpio(gpio_info* target_gpio);

        int readdbusgpio(gpio_info* target_gpio);

        bool ischassisgpio(gpio_info* target_gpio);

};
