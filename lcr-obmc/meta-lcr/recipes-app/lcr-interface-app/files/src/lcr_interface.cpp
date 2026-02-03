#include "../include/lcr_interface.hpp"

using json = nlohmann::json;

CLI::CLI() : bus(sdbusplus::bus::new_default()) {};

CLI::~CLI() {;};

int CLI::process_argument(int nargs, char* args[]) {

    if (nargs > 1) {
        std::string arg = std::string(args[1]);
        if (arg == "set"){
            if (nargs == 5) {
                double setpoint = std::stod(std::string(args[5]));
                set_threshold(std::string(args[2]), std::string(args[3]), std::string(args[4]), setpoint);
            } else {
                std::cout << "Incorrect number of arguments for set command" << std::endl;
            }
        }

    } else {
        std::string arg = std::string(args[1]);
        std::transform(arg.begin(), arg.end(), arg.begin(),
                    [](unsigned char c){ return std::tolower(c); });

        if (arg == "temperature" || arg == "temperatures") {
            display_temps();
        } else if (arg == "voltage" || arg == "voltages") {
            display_ADC();
        } else if (arg == "fan" || arg == "fans") {
            display_fans();
        } else if (arg == "all") {
            display_all();
        } else if (arg == "alarm" || arg == "alarms") {
            display_alarms();
        } else if (arg == "set") {
            std::cout << "more arguments required for the set command" << std::endl;
        }else {
            std::cout << "unrecognized command" << std::endl;
        }
    }

    return 0;
}

int CLI::display_all() {
    std::cout << "_FANS_" << std::endl;
    display_fans();
    std::cout << "_TEMPERATURES_" << std::endl;
    display_temps();
    std::cout << "_VOLTAGES_" << std::endl;
    display_ADC();
    std::cout << "_ALARMS_" << std::endl;
    display_alarms();
    return 0;
}

int CLI::display_fans() {
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string pwmstub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tachstub = "/xyz/openbmc_project/sensors/fan_tach";

    std::string property = "Value";

    std::cout << "____________________________________________________________________________________________" << std::endl;
    std::string fanName;

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, pwmstub, sense_interface, 0)) {

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            fanName = path.substr(1 + itr);
        }

        std::cout << fanName << " percentage: " << phosphor::interface::util::getProperty<double>(bus, path, sense_interface, property) << std::endl;
    };

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tachstub, sense_interface, 0)) {
        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            fanName = path.substr(1 + itr);
        }
        std::cout << fanName << "\t Value: " << phosphor::interface::util::getProperty<double>(bus, path, sense_interface, property)  << " RPM"
            << ",\t Min: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
            ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
            ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
            ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
            std::endl;
    };
    std::cout << "____________________________________________________________________________________________" << std::endl;
    return 0;
}

int CLI::display_temps() {
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string service = "xyz.openbmc_project.temperatureHwmon";
    std::string stub = "/xyz/openbmc_project/sensors/temperature";
    
    std::string property = "Value";
    std::cout << "____________________________________________________________________________________________" << std::endl;
    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string tempName;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            tempName = path.substr(1 + itr);
        }
        std::cout << tempName << "\t Value: " << phosphor::interface::util::getProperty<double>(bus, path, interface, property) << " C" << 
            ",\t Min: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
            ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
            ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
            ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
            std::endl;
    };
    std::cout << "____________________________________________________________________________________________" << std::endl;
    return 0;
}

int CLI::display_ADC() {
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string service = "xyz.openbmc_project.ADCHwmon";
    std::string stub = "/xyz/openbmc_project/sensors/voltage";
    
    std::string property = "Value";

    std::cout << "____________________________________________________________________________________________" << std::endl;

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string ADCname;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            ADCname = path.substr(1 + itr);
        }
        std::cout << ADCname << "\t Value: " << phosphor::interface::util::getProperty<double>(bus, path, interface, property) << " V" <<
            ",\t Min: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
            ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
            ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
            ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
            std::endl;
    };
    std::cout << "____________________________________________________________________________________________" << std::endl;
    return 0;
}

int CLI::display_alarms() {
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string voltage_service = "xyz.openbmc_project.ADCHwmon";
    std::string voltage_stub = "/xyz/openbmc_project/sensors/voltage";
    std::string temperature_service = "xyz.openbmc_project.temperatureHwmon";
    std::string temperature_stub = "/xyz/openbmc_project/sensors/temperature";
    std::string pwm_stub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tach_stub = "/xyz/openbmc_project/sensors/fan_tach";

    std::cout << "Temperatures" << std::endl;

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, temperature_stub, threshold_interface, 0)) {
        std::string tempName;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            tempName = path.substr(1 + itr);
        }
        std::cout << tempName << "\tMin: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
        std::endl;
    }

    std::cout << "Voltages" << std::endl;

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, voltage_stub, threshold_interface, 0)) {
        std::string ADCName;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            ADCName = path.substr(1 + itr);
        }
        std::cout << ADCName << "\tMin: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
        std::endl;
    }

    std::cout << "Fans" << std::endl;

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tach_stub, threshold_interface, 0)) {
        std::string fanName;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            fanName = path.substr(1 + itr);
        }
        std::cout << fanName << "\tMin: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        ",\t Low Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        ",\t Max: " << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        ",\t High Alarm: " << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
        std::endl;
    }

    return 0;
}

int CLI::set_threshold(std::string type, std::string itemname, std::string highorlow, double value) {
    std::ifstream input_file("/usr/share/thresholds/thresholds.json");
    json j;
    input_file >> j;
    input_file.close();

    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string voltage_service = "xyz.openbmc_project.ADCHwmon";
    std::string voltage_stub = "/xyz/openbmc_project/sensors/voltage";
    std::string temperature_service = "xyz.openbmc_project.temperatureHwmon";
    std::string temperature_stub = "/xyz/openbmc_project/sensors/temperature";
    std::string pwm_stub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tach_stub = "/xyz/openbmc_project/sensors/fan_tach";

    std::string name;

    std::transform(type.begin(), type.end(), type.begin(),
                    [](unsigned char c){ return std::tolower(c); });

    std::transform(highorlow.begin(), highorlow.end(), highorlow.begin(),
                    [](unsigned char c){ return std::tolower(c); });

    if (type == "fan" || type == "fans") {
        std::cout << "set called for " << itemname << std::endl;
        for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tach_stub, threshold_interface, 0)) {
    
            auto itr = path.rfind("/");
            if (itr != std::string::npos && itr < path.size())
            {
                name = path.substr(1 + itr);
            }
            if (name == itemname) {
                if (highorlow == "high") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh"), std::move(value));
                } else if (highorlow == "low") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalLow"), std::move(value));
                } else {
                    std::cout << "argument must be \"High\" or \"Low\"" << std::endl; 
                }
            }
        };
    } else if (type == "voltage") {
        std::cout << "set called for " << itemname << std::endl;
        for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, voltage_stub, threshold_interface, 0)) {
            auto itr = path.rfind("/");
            if (itr != std::string::npos && itr < path.size())
            {
                name = path.substr(1 + itr);
            }
            if (name == itemname) {
                if (highorlow == "high") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh"), std::move(value));
                    j[itemname]["max"] = value;
                } else if (highorlow == "low") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalLow"), std::move(value));
                    j[itemname]["min"] = value;
                } else {
                    std::cout << "argument must be \"High\" or \"Low\"" << std::endl; 
                }
            }
        };
    } else if (type == "temperature") {
        std::cout << "set called for " << itemname << std::endl;
        for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, temperature_stub, threshold_interface, 0)) {
            
            auto itr = path.rfind("/");
            if (itr != std::string::npos && itr < path.size())
            {
                name = path.substr(1 + itr);
            }
            if (name == itemname) {
                if (highorlow == "high") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh"), std::move(value));
                    j["TempLimits"]["max"] = value;
                } else if (highorlow == "low") {
                    std::cout << "setting " << name << " " << highorlow << " threshold to " << value << std::endl;
                    phosphor::interface::util::setProperty<double>(bus, path, threshold_interface, std::string("CriticalLow"), std::move(value));
                    j["TempLimits"]["min"] = value;
                } else {
                    std::cout << "argument must be \"High\" or \"Low\"" << std::endl; 
                }
            }
        };
    } else {
        std::cout << "Unknown type" << std::endl;
    }

    std::ofstream output_file("/usr/share/thresholds/thresholds.json");

    output_file << j.dump(4);
    output_file.close();
    
    return 0;
}