#include "../include/mandatorySensors.hpp"


// adding a function for setting alarms with try/catch:
bool alarm_check(sdbusplus::bus::bus& bus_ref, std::string stub, std::string interface, int depth) {
    bool alarmactive = false;

    try {
        for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus_ref, stub, interface, depth)) {
            if (phosphor::interface::util::getProperty<bool>(bus_ref, path, interface, std::string("CriticalAlarmLow")) || 
            phosphor::interface::util::getProperty<bool>(bus_ref, path, interface, std::string("CriticalAlarmHigh"))) {
                alarmactive = true;
            }
        }
    } catch (...) {
        return alarmactive;
    }
    
    return alarmactive;
}

/*
####################################################
FRU HEALTH STATE DEFINITION
####################################################
*/

FRUHealthState::FRUHealthState(sdbusplus::bus::bus& bus_ref) : bus(bus_ref) {}

FRUHealthState::~FRUHealthState() {}

void FRUHealthState::expose_sensor() {
    std::string path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_FRU_HEALTH";

    sensor = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface>>(bus, path.c_str());

    sensor->associations({
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
    });
}

uint8_t FRUHealthState::get_all_alarms_status() {
    
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string pwmstub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tachstub = "/xyz/openbmc_project/sensors/fan_tach";
    std::string tempstub = "/xyz/openbmc_project/sensors/temperature";
    std::string adcstub = "/xyz/openbmc_project/sensors/voltage";

    bool alarmactive = false;
    alarmactive = alarm_check(bus, tempstub, threshold_interface, 0);

    alarmactive = alarm_check(bus, tachstub, threshold_interface, 0);

    alarmactive = alarm_check(bus, adcstub, threshold_interface, 0);

    if (alarmactive) {
        return 0x02;
    }
    return 0x01;
}

void FRUHealthState::update_interface(){
    sensor->value(get_all_alarms_status());
}

/*
####################################################
FRU VOLTAGE STATE DEFINITION
####################################################
*/

FRUVoltageState::FRUVoltageState(sdbusplus::bus::bus& bus_ref) : bus(bus_ref) {}

FRUVoltageState::~FRUVoltageState() {}

void FRUVoltageState::expose_sensor() {
    std::string path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_VOLTAGE";

    sensor = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface>>(bus, path.c_str());

    sensor->associations({
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
    });
}

uint8_t FRUVoltageState::get_all_alarms_status() {
    
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string adcstub = "/xyz/openbmc_project/sensors/voltage";

    bool alarmactive = false;
    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, adcstub, threshold_interface, 0)) {
        if (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) || 
        phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"))) {
            alarmactive = true;
        }
    }
    if (alarmactive) {
        return 0x02;
    }
    return 0x01;
}

void FRUVoltageState::update_interface(){
    sensor->value(get_all_alarms_status());
}

/*
####################################################
FRU TEMPERATURE STATE DEFINITION
####################################################
*/

FRUTemperatureState::FRUTemperatureState(sdbusplus::bus::bus& bus_ref) : bus(bus_ref) {}

FRUTemperatureState::~FRUTemperatureState() {}

void FRUTemperatureState::expose_sensor() {
    std::string path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_TEMPERATURE";

    sensor = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface>>(bus, path.c_str());

    sensor->associations({
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
    });
}

uint8_t FRUTemperatureState::get_all_alarms_status() {
    
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string tempstub = "/xyz/openbmc_project/sensors/temperature";

    bool alarmactive = false;
    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tempstub, threshold_interface, 0)) {
        if (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) || 
        phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"))) {
            alarmactive = true;
        }
    }
    if (alarmactive) {
        return 0x02;
    }
    return 0x01;
}

void FRUTemperatureState::update_interface(){
    sensor->value(get_all_alarms_status());
}







