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

static constexpr size_t thresholdTypeCodes = 0;

enum class ThresholdTypeCodes : uint8_t
{
    lnc_low  = 0x00,
    lnc_high = 0x01,
    lcr_low  = 0x02,
    lcr_high = 0x03,
    lnr_low  = 0x04,
    lnr_high = 0x05,
    unc_low  = 0x06,
    unc_high = 0x07,
    ucr_low  = 0x08,
    ucr_high = 0x09,
    unr_low  = 0x0A,
    unr_high = 0x0B
};

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
    std::string sensorName;
    std::string directory;
    std::string directory_path;
    bool first;
    directoryContents conts;
    directoryVals vals;
};

class Temp_Sensor {
    public: 
        Temp_Sensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~Temp_Sensor();

        Temp_Sensor(const Temp_Sensor&) = delete;
        Temp_Sensor& operator=(const Temp_Sensor&) = delete;

        Temp_Sensor(Temp_Sensor&&) = default;
        Temp_Sensor& operator=(Temp_Sensor&&) = default;

        std::vector<std::string> explore_dir();
        int get_file_conts();
        nlohmann::json readJson(std::string path);
        
        void expose_reading(const sensorElement& tempS);
        void update_reading();
        void async_readtemp(const std::string& temp_name, std::function<void(double, double)> callback);
        void async_update_bus(const std::string& dirName, double old_val, double new_val);
        void sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3);

        void schedule_update();
        
    private:
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_value_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_assoc_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_crit_thres_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_warn_thres_iface;
        boost::asio::steady_timer timer;
        std::unordered_map<std::string, sensorElement> sensorItems;

        nlohmann::json limitJson;
        nlohmann::json configJson;
};