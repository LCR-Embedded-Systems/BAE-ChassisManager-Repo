#include "../include/ADC_manipulation.hpp"

std::ofstream ADC_dbgfile("ADC_lcr.dbg.log", std::ios::app);

struct ADCpaths
{
    std::string V12path = "/sys/bus/iio/devices/iio:device0/in_voltage9_raw";
    std::string V3_3path = "/sys/bus/iio/devices/iio:device0/in_voltage10_raw";
    std::string V5path = "/sys/bus/iio/devices/iio:device0/in_voltage11_raw";
    std::string V3_3Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage12_raw";
    std::string Vp12Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage13_raw";
    std::string Vn12Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage14_raw";
};

nlohmann::json ADC_sensor::readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

ADC_sensor::ADC_sensor() : bus(sdbusplus::bus::new_default()),
                             object_manager(std::make_unique<ObjectManagerIface>(bus, "/xyz/openbmc_project/sensors")) 
{
    bus.request_name("xyz.openbmc_project.ADCHwmon");
    
    limitJson = readJson("/usr/share/thresholds/thresholds.json");

    ADCpaths ADCpaths;
    
    ADCs.push_back(std::make_unique<ADC_element>("12V_Rail", ADCpaths.V12path, 12.0));
    ADCs.push_back(std::make_unique<ADC_element>("3_3V_Rail", ADCpaths.V3_3path, 3.3));
    ADCs.push_back(std::make_unique<ADC_element>("5V_Rail", ADCpaths.V5path, 5.0));
    ADCs.push_back(std::make_unique<ADC_element>("3_3auxV_Rail", ADCpaths.V3_3Auxpath, 3.3));
    ADCs.push_back(std::make_unique<ADC_element>("12pauxV_Rail", ADCpaths.Vp12Auxpath, 12.0));
    ADCs.push_back(std::make_unique<ADC_element>("12nauxV_Rail", ADCpaths.Vn12Auxpath, 12.0));

    calculate_cs();
    update_readings();
    expose_readings();
}

ADC_sensor::~ADC_sensor()
{
    ;
}

void ADC_sensor::calculate_cs() {
    for (const auto& ADC : ADCs) {
        ADC->scale_coeff = (ADC->V_target / 2047);
    }
}

int ADC_sensor::update_readings() {

    for (const auto& ADC : ADCs) {
        ADC->raw_reading = std::stoi(readFile(ADC->path));
        ADC->V_current = ADC->scale_coeff * ADC->raw_reading;
    }
    // ADC_dbgfile << "[" << currentTimestamp() << "] " << "LCR:ADC: updating ADC readings" << std::endl;
    return 0;
}

int ADC_sensor::expose_readings() {

    for (const auto& ADC : ADCs) {

        std::string path = "/xyz/openbmc_project/sensors/voltage/" + ADC->name;

        auto sensor = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());

        sensor->value(0);

        sensor->minValue(0);
        sensor->maxValue(24);

        sensor->unit(ValueIface::Unit::Volts);

        sensor->associations({
            {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
        });

        // sensor->criticalHigh((ADC->V_target) + (0.05 * ADC->V_target));
        // sensor->criticalLow((ADC->V_target) - (0.05 * ADC->V_target));
        sensor->criticalHigh(limitJson[ADC->name]["max"]);
        sensor->criticalLow(limitJson[ADC->name]["min"]);
        sensor->criticalAlarmHigh(false);
        sensor->criticalAlarmLow(false);

        // Store in map so it persists
        sensors[ADC->name] = std::move(sensor);

    }
    
    return 0;

};

void ADC_sensor::send_all_readings() {

    limitJson = readJson("/usr/share/thresholds/thresholds.json");
    bus.process_discard();
    bus.wait(std::chrono::milliseconds(1000));

    for (const auto& ADC : ADCs) {
        std::string voltage_stub = "/xyz/openbmc_project/sensors/voltage/" + ADC->name;
        std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";

        // double lowthresh = phosphor::interface::util::getProperty<double>(bus, voltage_stub, threshold_interface, "CriticalLow");
        // double highthresh = phosphor::interface::util::getProperty<double>(bus, voltage_stub, threshold_interface, "CriticalHigh");
        sensors[ADC->name]->criticalHigh(limitJson[ADC->name]["max"]);
        sensors[ADC->name]->criticalLow(limitJson[ADC->name]["min"]);
        double lowthresh = limitJson[ADC->name]["min"];
        double highthresh = limitJson[ADC->name]["max"];

        sensors[ADC->name]->value((ADC->V_current));
        if (ADC->V_current > highthresh) {
            sensors[ADC->name]->criticalAlarmHigh(true);
        } else {
            sensors[ADC->name]->criticalAlarmHigh(false);
        }
        
        if (ADC->V_current < lowthresh) {
            sensors[ADC->name]->criticalAlarmLow(true);
        } else {
            sensors[ADC->name]->criticalAlarmLow(false);
        }
    }

    

}

ADC_element* ADC_sensor::getSensorByName(const std::string& name) {
    for (const auto& ADC : ADCs) {
        if (ADC->name == name) {
            return ADC.get();
        }
    }
    return nullptr;
}