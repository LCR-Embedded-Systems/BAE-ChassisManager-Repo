#include "global.hpp"
#include "pid.hpp"

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
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface = sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;

using temp_name = std::string;

struct temperature_sensor {
    std::string path;
    temp_name name;
    double value;
};

struct fan {
    std::string path;
    std::string name;
    int pwm;
    int tach;
};

//fan loop controller definition
//TODO: create handling for both linear and PID controller schemas

class fan_loop
{

    public:

        fan_loop();
        ~fan_loop();

        int read_temperatures();
        int get_fan_pwms();
        int get_temp_thresholds();
        void initialize_temps();

        void initialize_controller();

        nlohmann::json readJson(std::string path);

        int control_loop();

        double get_ts();

    private:

        bool linear;

        double ts;

        double temp_min;
        double temp_max;
        double setpoint;
        
        std::vector<std::unique_ptr<temperature_sensor>> temps;
        std::vector<std::unique_ptr<fan>> fans;
        std::vector<std::string> pwm_paths;

        std::map<temp_name, fan> temp_fan_map;

        std::unique_ptr<pid_control::ec::pid_info_t> controller;

        sdbusplus::bus::bus bus;

        std::map<std::string, std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>> fan_controllers;

        std::unique_ptr<ObjectManagerIface> object_manager;

};
