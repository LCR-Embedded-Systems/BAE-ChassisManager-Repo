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
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface = sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;

struct ADC_element
{
    std::string name;
    std::string path;
    double V_target;
    int raw_reading;
    double scale_coeff;
    double V_current;
    ADC_element(const std::string& n, const std::string& p, double target) : name(n), path(p), V_target(target) {}
};

class ADC_sensor
{

    public:

        ADC_sensor();
        ~ADC_sensor();
        int update_readings();
        int get_reading(ADC_element ADC);
        int expose_readings();
        void send_all_readings();
        void calculate_cs();
        ADC_element* getSensorByName(const std::string& name);

        nlohmann::json readJson(std::string path);

    private:

        std::vector<std::unique_ptr<ADC_element>> ADCs;
        
        sdbusplus::bus::bus bus;

        std::map<std::string, std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>> sensors;

        std::unique_ptr<ObjectManagerIface> object_manager;

        nlohmann::json limitJson;

};
