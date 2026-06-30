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

using ValueIface =         sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface =   sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface =     sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;

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

struct fan_element {
    std::string pwm_path;
    std::string pwmenable_path;
    std::string tach_path;
    std::string fanenable_path;
    std::string fanfault_path;
    std::string fantarget_path;
    std::string name;
    uint64_t pwm;
    int tach;
    int num;
    int pwm_enable;
    int fan_enable;
    bool first;
    fan_element(std::string pwm_p, std::string tach_p, std::string pen_p, std::string fen_p, std::string ffault_p, 
        std::string ftarg_p, std::string n, int pwm_init, int tach_init, int number, int pen, int fen, bool firsten) : pwm_path(pwm_p), 
        tach_path(tach_p), pwmenable_path(pen_p), fanenable_path(fen_p), fanfault_path(ffault_p), 
        fantarget_path(ftarg_p), name(n), pwm(pwm_init), tach(tach_init), num(number), pwm_enable(pen), fan_enable(fen), first(firsten) {}
};

class fan_monitor
{

    public:

        fan_monitor(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~fan_monitor();

        int get_fan_pwms();

        nlohmann::json readJson(std::string path);
        int read_controller_val(std::string path);

        void fan_enable(std::string fen_path);

        void update_readings();

        void async_update_pwm(const std::string& fan_name, std::function<void(uint64_t, uint64_t)> callback);
        void async_readtach(const std::string& fan_name, std::function<void(int, int)> callback);
        void async_update_bus_pwm(const std::string& adc_name, uint64_t old_val, uint64_t new_val);
        void async_update_bus_tach(const std::string& adc_name, int old_val, int new_val);

        void sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3);

        void expose_readings();

        void schedule_update();

    private:
        
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;

        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_pwm_control_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_pwm_sense_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_pwm_assoc_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_assoc_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_crit_thresh_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_warn_thresh_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_sense_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_avail_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_tach_oper_iface;

        boost::asio::steady_timer timer;

        std::unordered_map<std::string, std::unique_ptr<fan_element>> fanItems;

        nlohmann::json limitJson;
        nlohmann::json configJson;

};
