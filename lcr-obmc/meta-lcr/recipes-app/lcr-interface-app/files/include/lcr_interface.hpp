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

class CLI
{
    public:
    
        CLI();
        ~CLI();

        void printsplash();

        int display_all();

        int display_fans();

        int display_temps();

        int display_ADC();

        int display_current();

        int display_power();

        int display_alarms();

        int display_services();

        int set_threshold(std::string type, std::string itemname, std::string highorlow, double value);

        int process_argument(int nargs, char* args[]);

    private:

        sdbusplus::bus::bus bus;
    
};
