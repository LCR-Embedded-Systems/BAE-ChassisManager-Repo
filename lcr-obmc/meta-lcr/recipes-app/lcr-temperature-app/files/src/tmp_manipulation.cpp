#include "../include/tmp_manipulation.hpp"


std::string getService(sdbusplus::bus_t& bus, const std::string& intf,
    const std::string& path)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService enter ");
    auto mapperCall =
    bus.new_method_call("xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetObject");

    mapperCall.append(path);
    mapperCall.append(std::vector<std::string>({intf}));

    auto mapperResponseMsg = bus.call(mapperCall);

    if (mapperResponseMsg.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService error in the mapper call");
        throw std::runtime_error("ERROR in mapper call");
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);

    if (mapperResponse.begin() == mapperResponse.end())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService error in the mapper response");
        throw std::runtime_error("ERROR in reading the mapper response");
    }

    return mapperResponse.begin()->first;
}

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

bool isDirConfig(std::string dir, nlohmann::json config) {
    for (auto& element : config["tempsensors"].items()) {
        if (dir == ("/sys/class/hwmon/" + element.key())) {
            return true;
        }
    }
    return false;
}

Temp_Sensor::Temp_Sensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
    sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{

    obj_server.add_manager("/xyz/openbmc_project/sensors");

    phosphor::logging::log<phosphor::logging::level::INFO>("Temp_Sensor initialization");

    limitJson = readJson("/usr/share/thresholds/thresholds.json");
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    std::vector<std::string> directories = explore_dir();
    for (std::string d : directories){
        if (isDirConfig(d, configJson)) {
            size_t pos = d.find_last_of('/');
            std::string result;
            if (pos != std::string::npos) {
                result = d.substr(pos + 1); // take everything after last "/"
            } else {
                result = d; // no "/" found, take whole string
            }
            sensorElement s;
            s.dirName = result;
            s.first = true;
            s.directory_path = d + "/";
            s.conts.reading_path = s.directory_path + "temp1_input";
            s.conts.max_path = s.directory_path + "temp1_max";
            s.conts.hyst_path = s.directory_path + "temp1_max_hyst";
            if (configJson["tempsensors"][result]["name"] == nullptr){
                s.sensorName = result;
            } else {
                s.sensorName = configJson["tempsensors"][result]["name"];
            }
            sensorItems[s.dirName] = s;
        } else {
            phosphor::logging::log<phosphor::logging::level::INFO>(("Temp_Sensor directory : " + d + " does not exist, exposing anyway").c_str());
            size_t pos = d.find_last_of('/');
            std::string result;
            if (pos != std::string::npos) {
                result = d.substr(pos + 1); // take everything after last "/"
            } else {
                result = d; // no "/" found, take whole string
            }
            sensorElement s;
            s.dirName = result;
            s.first = true;
            s.directory_path = d + "/";
            s.conts.reading_path = s.directory_path + "temp1_input";
            s.conts.max_path = s.directory_path + "temp1_max";
            s.conts.hyst_path = s.directory_path + "temp1_max_hyst";
            s.sensorName = result;
            sensorItems[s.dirName] = s;
        }
    }

    get_file_conts();
    
    for (auto& [name, s] : sensorItems) {
        expose_reading(s);
    }

}

Temp_Sensor::~Temp_Sensor() 
{
    
}

int Temp_Sensor::get_file_conts() 
{
    for (auto& [name, s] : sensorItems) {    
        std::ifstream readingfilemax(s.conts.max_path);
        std::ifstream readingfilehyst(s.conts.hyst_path);

        if (!readingfilemax || !readingfilehyst) {
            std::cerr << "Failed to open file\n";
            return 1;
        }

        readingfilemax >> s.vals.temp1_max;
        readingfilehyst >> s.vals.temp1_max_hyst;

    }
    return 0;
}

void Temp_Sensor::expose_reading(const sensorElement& tempS) {

    // Path for the sensor
    std::string path = "/xyz/openbmc_project/sensors/temperature/" + tempS.sensorName;

    std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Value");
    std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Association.Definitions");
    std::unique_ptr<sdbusplus::asio::dbus_interface> crit_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Critical");
    std::unique_ptr<sdbusplus::asio::dbus_interface> warn_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Warning");

    phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path).c_str());

    value_iface->register_property_r<double>(
        "Value",
        sdbusplus::vtable::property_::emits_change,
        [this, name = tempS.dirName](const double&) -> double {
            // Getter: Retrieve from map
            auto it = sensorItems.find(name);
            if (it != sensorItems.end()) {
                return (((double)it->second.vals.cur_reading) / 1000);
            }
            return -253.0;  // Fallback
        }
    );

    // Register other required properties for Sensor.Value with defaults
    value_iface->register_property_r<double>(
        "MaxValue",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return 120.0; }
    );
    value_iface->register_property_r<double>(
        "MinValue",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return -60.0; }
    );
    value_iface->register_property_r<std::string>(
        "Unit",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.DegreesC"); }
    );
    value_iface->register_property_r<int64_t>(
        "Scale",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return int64_t(0); }
    );

    crit_thres_iface->register_property_r<double>(
        "CriticalHigh",
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return double(limitJson["TempLimits"]["critmax"]); }
    );
    crit_thres_iface->register_property_r<double>(
        "CriticalLow",
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return double(limitJson["TempLimits"]["critmin"]); }
    );
    crit_thres_iface->register_property_r<bool>(
        "CriticalAlarmHigh",
        sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
        [this, name = tempS.dirName](const auto&) {
            auto it = sensorItems.find(name);
            if (it != sensorItems.end()) {
                double cur_val = it->second.vals.cur_reading / 1000.0;
                return cur_val > limitJson["TempLimits"]["critmax"];
            }
            return false;
        }
    );
    crit_thres_iface->register_property_r<bool>(
        "CriticalAlarmLow",
        sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
        [this, name = tempS.dirName](const auto&) {
            auto it = sensorItems.find(name);
            if (it != sensorItems.end()) {
                double cur_val = it->second.vals.cur_reading / 1000.0;
                return cur_val < limitJson["TempLimits"]["critmin"];
            }
            return false;
        }
    );

    warn_thres_iface->register_property_r<double>(
        "WarningHigh",
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return double(limitJson["TempLimits"]["warnmax"]); }
    );
    warn_thres_iface->register_property_r<double>(
        "WarningLow",
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return double(limitJson["TempLimits"]["warnmin"]); }
    );
    warn_thres_iface->register_property_r<bool>(
        "WarningAlarmHigh",
        sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
        [this, name = tempS.dirName](const auto&) {
            auto it = sensorItems.find(name);
            if (it != sensorItems.end()) {
                double cur_val = it->second.vals.cur_reading / 1000.0;
                return cur_val > limitJson["TempLimits"]["warnmax"];
            }
            return false;
        }
    );
    warn_thres_iface->register_property_r<bool>(
        "WarningAlarmLow",
        sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
        [this, name = tempS.dirName](const auto&) {
            auto it = sensorItems.find(name);
            if (it != sensorItems.end()) {
                double cur_val = it->second.vals.cur_reading / 1000.0;
                return cur_val < limitJson["TempLimits"]["warnmin"];
            }
            return false;
        }
    );

    assoc_iface->register_property_r<std::vector<std::tuple<std::string, std::string, std::string>>>(
        "Associations",
        sdbusplus::vtable::property_::const_,
        [](const auto&) {
            return std::vector<std::tuple<std::string, std::string, std::string>>{
                {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
            };
        }
    );

    value_iface->initialize();
    crit_thres_iface->initialize();
    warn_thres_iface->initialize();
    assoc_iface->initialize();

    m_value_iface[tempS.dirName] = std::move(value_iface);
    m_crit_thres_iface[tempS.dirName] = std::move(crit_thres_iface);
    m_warn_thres_iface[tempS.dirName] = std::move(warn_thres_iface);
    m_assoc_iface[tempS.dirName] = std::move(assoc_iface);
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_reading exit");
}

void Temp_Sensor::update_reading() 
{
    if (sensorItems.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map (use vector of keys for ordering if needed; here assuming order doesn't matter)
    std::vector<std::string> keys;
    for (const auto& kv : sensorItems) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            return;
        }
        const std::string& temp_name = keys[*idx];
        ++(*idx);
        async_readtemp(temp_name, [this, check_next_ptr, temp_name](double old_value, double new_value) {
            async_update_bus(temp_name, old_value, new_value);
            (*check_next_ptr)();  // Proceed to next temp sensor
        });
    };
    (*check_next_ptr)();  // Start chain

}

void Temp_Sensor::async_readtemp(const std::string& temp_name, std::function<void(double, double)> callback) {
    boost::asio::post(timer.get_executor(), [this, temp_name, callback]() {
        double new_value = -253.0;  // Error value
        try {
            auto it = sensorItems.find(temp_name);
            if (it == sensorItems.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>("async_readtemp callback failed to find sensor item");
                throw std::runtime_error("temp sensor not found");
            }
            sensorElement* temp_ptr = &it->second;  // Pointer to modifiable sensor element in map

            double old_value = ((double)temp_ptr->vals.cur_reading) / 1000.0;

            std::ifstream readingfile(temp_ptr->conts.reading_path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << temp_ptr->conts.reading_path << "\n";
                callback(-253.0, -253.0);
            }

            readingfile >> temp_ptr->vals.cur_reading;

            new_value = (((double)temp_ptr->vals.cur_reading)/1000);  // Get updated status
            callback(old_value, new_value);
        } catch (const std::exception& e) {
            // On error, don't update cur_reading (already not updated), log, and callback with error value
            double error_value = -253.0;
            double old_value = -253.0;  // Or fetch actual old if needed; using error for consistency
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read sensor " + temp_name + ": " + e.what()).c_str());
            callback(old_value, error_value);
        }
    });
}

void Temp_Sensor::async_update_bus(const std::string& dirName, double old_val, double new_val) {
    auto vit = m_value_iface.find(dirName);
    auto tit = m_crit_thres_iface.find(dirName);
    auto it = sensorItems.find(dirName);
    sensorElement* temp_ptr = &it->second;
    if (vit == m_value_iface.end() || !vit->second || tit == m_crit_thres_iface.end() || !tit->second) {
        return;  // No interfaces for this sensor
    }

    // Emit change signals (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, viface = vit->second.get(), tiface = tit->second.get(), dirName, temp_ptr, old_val, new_val]() {

        if (viface) {
            viface->signal_property("Value");  // Always signal Value change
        }
        nlohmann::json local_limitJson = readJson("/usr/share/thresholds/thresholds.json");

        // Extract thresholds with error handling (see mitigations below)
        double highthresh = 0.0;  // Default/fallback
        double lowthresh = 0.0;   // Default/fallback
        double warnhighthresh = 0.0;  // Default/fallback
        double warnlowthresh = 0.0;   // Default/fallback
        try {
            highthresh = local_limitJson["TempLimits"]["critmax"];
            lowthresh = local_limitJson["TempLimits"]["critmin"];
            warnhighthresh = local_limitJson["TempLimits"]["warnmax"];
            warnlowthresh = local_limitJson["TempLimits"]["warnmin"];
        } catch (const nlohmann::json::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to parse thresholds JSON: " + std::string(e.what())).c_str());
            return;
        }
        // Check and signal alarms only if state changed
        bool old_warn_alarm_high = old_val > warnhighthresh;
        bool new_warn_alarm_high = new_val > warnhighthresh;
        uint8_t event_data1 = new_warn_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::unc_high) : static_cast<uint8_t>(ThresholdTypeCodes::unc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_high != new_warn_alarm_high && tiface || (new_warn_alarm_high && temp_ptr->first && tiface)) {
            sendPlatformEvent(dirName, new_warn_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmHigh");
        }

        bool old_warn_alarm_low = old_val < warnlowthresh;
        bool new_warn_alarm_low = new_val < warnlowthresh;
        event_data1 = new_warn_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lnc_high) : static_cast<uint8_t>(ThresholdTypeCodes::lnc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_low != new_warn_alarm_low && tiface && old_val != -253.0 && new_val != -253.0 && old_val > -10000 || (new_warn_alarm_low && temp_ptr->first && tiface && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Warning Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(dirName, new_warn_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmLow");
        }
        
        bool old_alarm_high = old_val > highthresh;
        bool new_alarm_high = new_val > highthresh;
        event_data1 = new_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::ucr_high) : static_cast<uint8_t>(ThresholdTypeCodes::ucr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_high != new_alarm_high && tiface || (new_alarm_high && temp_ptr->first && tiface)) {
            sendPlatformEvent(dirName, new_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmHigh");
        }

        bool old_alarm_low = old_val < lowthresh;
        bool new_alarm_low = new_val < lowthresh;
        event_data1 = new_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lcr_high) : static_cast<uint8_t>(ThresholdTypeCodes::lcr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_low != new_alarm_low && tiface && old_val != -253.0 && new_val != -253.0 && old_val > -10000 || (new_alarm_low && temp_ptr->first && tiface && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Critical Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(dirName, new_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmLow");
        }
    });
    temp_ptr->first = false;
}


// Schedule the next periodic update
void Temp_Sensor::schedule_update() {
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("Temp_Sensor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->update_reading();
    });
}

void Temp_Sensor::sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent enter");
    [[maybe_unused]] uint16_t generatorID = 0;

    //sysgeneratorID, evmRev, sensorType, sensorNum, eventType, eventData1, eventData2, eventData3

    generatorID = (0 << 12)      // Channel
        | (0x0 << 10)             // Reserved
        | ((0 & 0x3) << 8) // Lun
        | (0x10 << 1);

    {// bit 7 of eventType: 0 = assert, 1 = deassert

        std::string sensorPath = "/temperature/" + dirName;

        std::vector<uint8_t> eventData{eventData1};
        if (eventData2.has_value())
            eventData.push_back(*eventData2);
        if (eventData3.has_value())
            eventData.push_back(*eventData3);

        try
        {
            sdbusplus::bus_t dbus = sdbusplus::bus::new_default();
            std::string service = getService(dbus,
            "xyz.openbmc_project.Logging.IPMI",
            "/xyz/openbmc_project/Logging/IPMI");

            auto msg = dbus.new_method_call(
            service.c_str(),
            "/xyz/openbmc_project/Logging/IPMI",
            "xyz.openbmc_project.Logging.IPMI",
            "IpmiSelAdd");

            // Parameters match the exact IpmiSelAdd D-Bus method:
            // Message (string), Path (object_path), SELData (array byte), Assert (bool), GeneratorID (uint16)
            msg.append("SEL Entry", sensorPath, eventData, assertEvent, generatorID);
            phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent sending dbus message");
            dbus.call(msg);
        }
        catch (const sdbusplus::exception_t& e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to log Platform Event to SEL: " + std::string(e.what())).c_str());
        }
    }
}