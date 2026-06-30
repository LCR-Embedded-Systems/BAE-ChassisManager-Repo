#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/bus/match.hpp>

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <optional>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Network/EthernetInterface/server.hpp>
#include <queue>
#include <vector>
#include <future>
    
#include <bitset>
#include <cmath>
#include <fstream>
#include <variant>
#include <vector>
#include <string>

#include <fstream>
#include <ostream>

#include <systemd/sd-bus.h>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

#include "global.hpp"

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface = sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;

/*
* Mandatory sensors is created in order to handle one of the most important functions of a vita 46 chassis manager, the get mandatory sensor
* numbers command. Classes will be built for each of the mantatory sensor number reports: FRU Operational State, IPMB Link, FRU Health, Voltage, 
* Temperature, Payload Test, and Payload Test Status.
* 
* 
* 
*/

class FRUVoltageSensor {
    //
    private: 
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface;
        std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface;
        double current_value = 0.0;
        double byte3 = 0.0;
        double byte4 = 0.0;
        boost::asio::steady_timer timer;
        void emit_value_changed();
        std::string sensor_path;

    public: 
        FRUVoltageSensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~FRUVoltageSensor();
        void async_get_all_alarms_status(std::function<void(uint8_t)> callback);
        void start_updates();
        void expose_sensor();
        void async_update_interface();
        void schedule_update();
};