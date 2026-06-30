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

class GpioMonitor
{

    public:
    
        GpioMonitor(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~GpioMonitor();

        bool readgpio(gpio_info* target_gpio);

        void initallpins();

        void readallpins();

        void expose_gpio(const gpio_info& target_gpio);
        void expose_gpios();

        void update_bus(gpio_info* target_gpio);

        void schedule_update();

        double get_ts();

        void async_readgpio(const std::string& gpio_name, std::function<void(bool, int)> callback);
        void async_readallpins();
        void async_update_bus(const std::string& gpio_name, bool new_status);

    private:

        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_value_iface;
        std::map<std::string, std::unique_ptr<sdbusplus::asio::dbus_interface>> m_assoc_iface;
        boost::asio::steady_timer timer;

        std::unordered_map<std::string, gpio_info> available_gpios;
        std::unordered_map<std::string, gpio_info> chassis_signals;

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

        double ts;
        
        nlohmann::json configJson;
};
