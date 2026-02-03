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


struct directoryContents {
    std::string reading_path;
    std::string max_path;
    std::string hyst_path;
};

struct directoryVals {
    int cur_reading;
    int temp1_max;
    int temp1_max_hyst;
};

struct sensorElement {
    std::string dirName;
    std::string directory;
    std::string directory_path;
    directoryContents conts;
    directoryVals vals;
};




class Temp_Sensor {
    public: 
        Temp_Sensor();
        ~Temp_Sensor();

        Temp_Sensor(const Temp_Sensor&) = delete;
        Temp_Sensor& operator=(const Temp_Sensor&) = delete;

        Temp_Sensor(Temp_Sensor&&) = default;
        Temp_Sensor& operator=(Temp_Sensor&&) = default;

        int update_reading();
        int get_reading(sensorElement sensor);
        int expose_reading(const std::string& name);
        int get_file_conts();
        std::string get_directory();
        void send_reading(sensorElement s);
        void send_all_readings();
        std::vector<std::string> explore_dir();

        std::vector<sensorElement> sensorItems;

        nlohmann::json readJson(std::string path);

    private:
        // Persistent D-Bus bus
        sdbusplus::bus::bus bus;

        // Map of sensors
        std::map<std::string, std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>> sensors;

        std::unique_ptr<ObjectManagerIface> object_manager;

        nlohmann::json limitJson;
};