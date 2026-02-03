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

class FRUOperationalState {
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        FRUOperationalState(sdbusplus::bus::bus& bus_ref);
        ~FRUOperationalState();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};

class SystemIPMBState {
    //
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        SystemIPMBState(sdbusplus::bus::bus& bus_ref);
        ~SystemIPMBState();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};

class FRUHealthState {
    //
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        FRUHealthState(sdbusplus::bus::bus& bus_ref);
        ~FRUHealthState();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};

class FRUVoltageState {
    //poll the alarms of voltage sensors. if alarm is active, adjust interface.
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        FRUVoltageState(sdbusplus::bus::bus& bus_ref);
        ~FRUVoltageState();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};

class FRUTemperatureState {
    //poll the alarms of temperature sensors. if alarm is active, adjust interface.
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public:
        FRUTemperatureState(sdbusplus::bus::bus& bus_ref);
        ~FRUTemperatureState();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface(); 
};

class FRUPayloadTestResults {
    //
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        FRUPayloadTestResults(sdbusplus::bus::bus& bus_ref);
        ~FRUPayloadTestResults();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};

class FRUPayloadTestStatus {
    //
    private: 
        std::string interface;
        std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface>> sensor;

    public: 
        FRUPayloadTestStatus(sdbusplus::bus::bus& bus_ref);
        ~FRUPayloadTestStatus();
        sdbusplus::bus::bus& bus;
        uint8_t get_all_alarms_status();
        void expose_sensor();
        void update_interface();
};
