#include "../include/tmp_manipulation.hpp"

std::ofstream temp_dbgfile("temp_lcr.dbg.log", std::ios::app);

namespace fs = std::filesystem;

nlohmann::json Temp_Sensor::readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    return read;
}

std::vector<std::string> Temp_Sensor::explore_dir() 
{
    std::vector<std::string> directories;
    std::string path = "/sys/class/hwmon/";
    for (const auto& entry : fs::directory_iterator(path)) {

        if (isValidDirectory(entry.path().string())) {

            directories.push_back(entry.path().string());
        }
    }
    return directories;
}

Temp_Sensor::Temp_Sensor() : bus(sdbusplus::bus::new_default()),
                             object_manager(std::make_unique<ObjectManagerIface>(bus, "/xyz/openbmc_project/sensors"))
{
    // temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: creating a new instance in directory " << m_directory << std::endl;

    bus.request_name("xyz.openbmc_project.temperatureHwmon");

    limitJson = readJson("/usr/share/thresholds/thresholds.json");

    std::vector<std::string> directories = explore_dir();
    for (std::string d : directories){
        temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: building sensor from directory " << d << std::endl;
        size_t pos = d.find_last_of('/');
        std::string result;
        if (pos != std::string::npos) {
            result = d.substr(pos + 1); // take everything after last "/"
        } else {
            result = d; // no "/" found, take whole string
        }
        sensorElement s;
        s.dirName = result;
        s.directory_path = d + "/";
        s.conts.reading_path = s.directory_path + "temp1_input";
        s.conts.max_path = s.directory_path + "temp1_max";
        s.conts.hyst_path = s.directory_path + "temp1_max_hyst";
        temp_dbgfile << "\t[" << currentTimestamp() << "] " << "LCR:TEMP: dirName: " << s.dirName << std::endl;
        sensorItems.push_back(s);
    }

    get_file_conts();
    update_reading();

    
    for (sensorElement s : sensorItems) {
        expose_reading(s.dirName);
    }

}

Temp_Sensor::~Temp_Sensor() 
{
    temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: closing temp manipulation" << std::endl;
}

int Temp_Sensor::get_file_conts() 
{
    for (sensorElement s : sensorItems) {    
        std::ifstream readingfilemax(s.conts.max_path);
        std::ifstream readingfilehyst(s.conts.hyst_path);

        if (!readingfilemax || !readingfilehyst) {
            std::cerr << "Failed to open file\n";
            return 1;
        }

        readingfilemax >> s.vals.temp1_max;
        readingfilehyst >> s.vals.temp1_max_hyst;

        temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: max " << s.vals.temp1_max << std::endl;
        temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: hyst " << s.vals.temp1_max_hyst << std::endl;
    }
    return 0;
}

int Temp_Sensor::update_reading() 
{
    for (sensorElement& s : sensorItems) {

        std::ifstream readingfile(s.conts.reading_path);


        if (!readingfile) {
            std::cerr << "Failed to open file in " << s.conts.reading_path << "\n";
            return 1;
        }

        readingfile >> s.vals.cur_reading;
        // temp_dbgfile << "\t[" << currentTimestamp() << "] " << "LCR:TEMP: reading " << s.vals.cur_reading << std::endl;
    };
    return 0;
}

int Temp_Sensor::expose_reading(const std::string& name) {

    // Path for the sensor
    std::string path = "/xyz/openbmc_project/sensors/temperature/" + name;

    // temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: making sensor object" << std::endl;
    // Create the sensor object on the bus
    auto sensor = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());

    // Initialize with 0°C
    // temp_dbgfile << "[" << currentTimestamp() << "] " << "LCR:TEMP: setting sensor value" << std::endl;
    sensor->value(0);

    // Set MinValue and MaxValue
    sensor->minValue(-40.0);
    sensor->maxValue(80.0);

    // Set Unit to Degrees Celsius
    sensor->unit(ValueIface::Unit::DegreesC);

    // Set the Associations property
    sensor->associations({
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
    });



    sensor->criticalHigh(limitJson["TempLimits"]["max"]);
    sensor->criticalLow(limitJson["TempLimits"]["min"]);
    sensor->criticalAlarmHigh(false);
    sensor->criticalAlarmLow(false);

    // Store in map so it persists
    sensors[name] = std::move(sensor);

    return 0;
}

void Temp_Sensor::send_reading(sensorElement s) {

    limitJson = readJson("/usr/share/thresholds/thresholds.json");

    std::string tempstub = "/xyz/openbmc_project/sensors/temperature/" + s.dirName;
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";

    // temp_dbgfile << "LCR:TEMP: get property with the following: " << threshold_interface << " " << tempstub << std::endl;

    // double lowthresh = phosphor::interface::util::getProperty<double>(bus, tempstub, threshold_interface, "CriticalLow");
    // double highthresh = phosphor::interface::util::getProperty<double>(bus, tempstub, threshold_interface, "CriticalHigh");
    sensors[s.dirName]->criticalHigh(limitJson["TempLimits"]["max"]);
    sensors[s.dirName]->criticalLow(limitJson["TempLimits"]["min"]);
    double lowthresh = limitJson["TempLimits"]["min"];
    double highthresh = limitJson["TempLimits"]["max"];

    bus.process_discard();
    bus.wait(std::chrono::milliseconds(1000));

    sensors[s.dirName]->value(((double)s.vals.cur_reading) / 1000);

    if ((((double)s.vals.cur_reading) / 1000) > highthresh) {
        sensors[s.dirName]->criticalAlarmHigh(true);
    } else {
        sensors[s.dirName]->criticalAlarmHigh(false);
    }
    if ((((double)s.vals.cur_reading) / 1000) < lowthresh) {
        sensors[s.dirName]->criticalAlarmLow(true);
    } else {
        sensors[s.dirName]->criticalAlarmLow(false);
    }

}

void Temp_Sensor::send_all_readings() {
    for (sensorElement s : sensorItems) {
        send_reading(s);
    }
}
