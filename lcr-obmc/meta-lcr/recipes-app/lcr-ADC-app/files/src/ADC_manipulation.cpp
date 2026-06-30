#include "../include/ADC_manipulation.hpp"


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

namespace fs = std::filesystem;

ADC_sensor::ADC_sensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,sdbusplus::asio::object_server& server_ref) : 
    conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    limitJson = readJson("/usr/share/thresholds/thresholds.json");
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    for (auto& adc_dir : configJson["adcsensors"].items()) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor directory " + adc_dir.key()).c_str());
        for (auto& rail : adc_dir.value().items()) {
            if (rail.key() != "bus" && rail.key() != "address") {
                std::string dir = configJson["adcsensors"][adc_dir.key()][rail.key()]["dir"].get<std::string>();
                std::string path = "/sys/bus/iio/devices/" + adc_dir.key() + "/" + dir;
                double targ = configJson["adcsensors"][adc_dir.key()][rail.key()]["target"].get<double>();
                if (fs::exists(path) && configJson["adcsensors"][adc_dir.key()][rail.key()]["present"]) {
                    sensorItems[rail.key()] = std::make_unique<ADC_element>(rail.key(), path, targ);
                    sensorItems[rail.key()]->init = true;
                    sensorItems[rail.key()]->type = configJson["adcsensors"][adc_dir.key()][rail.key()]["type"].get<std::string>();
                    if (configJson["adcsensors"][adc_dir.key()][rail.key()].contains("nominal")) {
                        sensorItems[rail.key()]->nominal = configJson["adcsensors"][adc_dir.key()][rail.key()]["nominal"].get<double>();
                        sensorItems[rail.key()]->cmax = configJson["adcsensors"][adc_dir.key()][rail.key()]["max"].get<double>();
                    } else {
                        sensorItems[rail.key()]->nominal = -1.0;
                    }
                } else {
                    phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor path : " + path + " does not exist").c_str());
                }
            }
        }
    }

    calculate_cs();
    expose_readings();
    
}

ADC_sensor::~ADC_sensor()
{
    ;
}

void ADC_sensor::calculate_cs() {
    for (const auto& [key, ADC] : sensorItems) {
        if (ADC->nominal >= 0 && ADC->cmax != 0) {
            ADC->scale_coeff = ADC->V_target / ((ADC->nominal / ADC->cmax) * 4095);
        } else {
            ADC->scale_coeff = ADC->V_target / 2047;
        }
    }
}

int ADC_sensor::expose_readings() {

    for (const auto& [key, ADC] : sensorItems) {

        // Path for the sensor
        std::string path = "/xyz/openbmc_project/sensors/" + ADC->type + "/" + ADC->name;

        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Value");
        std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Association.Definitions");
        std::unique_ptr<sdbusplus::asio::dbus_interface> crit_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Critical");
        std::unique_ptr<sdbusplus::asio::dbus_interface> warn_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Warning");

        phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path).c_str());

        value_iface->register_property_r<double>(
            "Value",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double&) -> double {
                // Getter: Retrieve from map
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->V_current);
                }
                return -253.0;  // Fallback
            }
        );

        // Register other required properties for Sensor.Value with defaults
        value_iface->register_property_r<double>(
            "MaxValue",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return 24.0; }
        );
        value_iface->register_property_r<double>(
            "MinValue",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return -24.0; }
        );

        if (ADC->type == "voltage") {
            value_iface->register_property_r<std::string>(
                "Unit",
                sdbusplus::vtable::property_::const_,
                [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.Volts"); }
            );
        } else if (ADC->type == "current") {
            value_iface->register_property_r<std::string>(
                "Unit",
                sdbusplus::vtable::property_::const_,
                [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.Amperage"); }
            );
        }

        value_iface->register_property_r<int64_t>(
            "Scale",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return int64_t(0); }
        );

        crit_thres_iface->register_property_r<double>(
            "CriticalHigh",
            sdbusplus::vtable::property_::const_,
            [this, &ADC](const auto&) { return double(limitJson[ADC->name]["critmax"]); }
        );
        crit_thres_iface->register_property_r<double>(
            "CriticalLow",
            sdbusplus::vtable::property_::const_,
            [this, &ADC](const auto&) { return double(limitJson[ADC->name]["critmin"]); }
        );
        crit_thres_iface->register_property_r<bool>(
            "CriticalAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val > std::abs(limitJson[name]["critmax"].get<double>());
                }
                return false;
            }
        );
        crit_thres_iface->register_property_r<bool>(
            "CriticalAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val < std::abs(limitJson[name]["critmin"].get<double>());
                }
                return false;
            }
        );

        warn_thres_iface->register_property_r<double>(
            "WarningHigh",
            sdbusplus::vtable::property_::const_,
            [this, &ADC](const auto&) { return double(limitJson[ADC->name]["warnmax"]); }
        );
        warn_thres_iface->register_property_r<double>(
            "WarningLow",
            sdbusplus::vtable::property_::const_,
            [this, &ADC](const auto&) { return double(limitJson[ADC->name]["warnmin"]); }
        );
        warn_thres_iface->register_property_r<bool>(
            "WarningAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val > std::abs(limitJson[name]["warnmax"].get<double>());
                }
                return false;
            }
        );
        warn_thres_iface->register_property_r<bool>(
            "WarningAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val < std::abs(limitJson[name]["warnmin"].get<double>());
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

        m_value_iface[ADC->name] = std::move(value_iface);
        m_crit_thres_iface[ADC->name] = std::move(crit_thres_iface);
        m_warn_thres_iface[ADC->name] = std::move(warn_thres_iface);
        m_assoc_iface[ADC->name] = std::move(assoc_iface);
        
    }
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_reading exit");
    return 0;

};

void ADC_sensor::update_readings() {

    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading enter");
    if (sensorItems.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map (use vector of keys for ordering if needed; here assuming order doesn't matter)
    std::vector<std::string> keys;
    for (const auto& kv : sensorItems) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    // phosphor::logging::log<phosphor::logging::level::INFO>("starting to check temperatures and update bus");
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading exit");
            return;
        }
        const std::string& adc_name = keys[*idx];
        ++(*idx);
        // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading starting the async_readtemp");
        async_readadc(adc_name, [this, check_next_ptr, adc_name](double old_value, double new_value) {
            // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading entering the async bus update");
            async_update_bus(adc_name, old_value, new_value);
            (*check_next_ptr)();  // Proceed to next adc sensor
        });
        // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading after the async_readtemp");
    };
    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading moving onto the next pointer");
    (*check_next_ptr)();  // Start chain

    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading exit");
}

void ADC_sensor::async_readadc(const std::string& adc_name, std::function<void(double, double)> callback) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc enter");
    boost::asio::post(timer.get_executor(), [this, adc_name, callback]() {
        // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc entering callback");
        double new_value = -29.0;  // Error value
        try {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc callback trying to find sensor item at name " + adc_name).c_str());
            auto it = sensorItems.find(adc_name);
            if (it == sensorItems.end()) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback failed to find sensor item");
                throw std::runtime_error("adc not found");
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback trying to find sensor item");
            ADC_element* adc_ptr = it->second.get();  // Pointer to modifiable sensor element in map

            double old_value = (adc_ptr->V_current);
            if (adc_ptr->init) {
                old_value = adc_ptr->V_target;
                adc_ptr->init = false;
            }

            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc attempting to get file");
            std::ifstream readingfile(adc_ptr->path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << adc_ptr->path << "\n";
                callback(-29.0, -29.0);
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc getting current reading");
        
            readingfile >> adc_ptr->raw_reading;
            adc_ptr->V_current = adc_ptr->scale_coeff * adc_ptr->raw_reading;

            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc calculating");
            new_value = adc_ptr->V_current;  // Get updated status
            
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback");
            callback(old_value, new_value);
        } catch (const std::exception& e) {
            // On error, don't update cur_reading (already not updated), log, and callback with error value
            double error_value = -29.0;
            double old_value = -29.0;  // Or fetch actual old if needed; using error for consistency
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read sensor " + adc_name + ": " + e.what()).c_str());
            callback(old_value, error_value);
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc exit");
}

void ADC_sensor::async_update_bus(const std::string& adc_name, double old_val, double new_val) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus enter");
    auto vit = m_value_iface.find(adc_name);
    auto tit = m_crit_thres_iface.find(adc_name);
    if (vit == m_value_iface.end() || !vit->second || tit == m_crit_thres_iface.end() || !tit->second) {
        return;  // No interfaces for this sensor
    }

    // Emit change signals (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, viface = vit->second.get(), tiface = tit->second.get(), adc_name, old_val, new_val]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus starting to set signals");
        if (viface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus attempting to update bus for " + adc_name).c_str());
            viface->signal_property("Value");  // Always signal Value change
        }
        nlohmann::json local_limitJson = readJson("/usr/share/thresholds/thresholds.json");

        // Extract thresholds with error handling (see mitigations below)
        double highthresh = 0.0;  // Default/fallback
        double lowthresh = 0.0;   // Default/fallback
        double warnhighthresh = 0.0;  // Default/fallback
        double warnlowthresh = 0.0;   // Default/fallback
        try {
            highthresh = local_limitJson[adc_name]["critmax"];
            lowthresh = local_limitJson[adc_name]["critmin"];
            warnhighthresh = local_limitJson[adc_name]["warnmax"];
            warnlowthresh = local_limitJson[adc_name]["warnmin"];
        } catch (const nlohmann::json::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to parse thresholds JSON: " + std::string(e.what())).c_str());
            return;
        }

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus finished setting signals");
        bool old_warn_alarm_high = std::abs(old_val) > std::abs(warnhighthresh);
        bool new_warn_alarm_high = std::abs(new_val) > std::abs(warnhighthresh);
        uint8_t event_data1 = new_warn_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::unc_high) : static_cast<uint8_t>(ThresholdTypeCodes::unc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_high != new_warn_alarm_high && tiface || (new_warn_alarm_high && first)) {
            sendPlatformEvent(adc_name, new_warn_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmHigh");
        }

        bool old_warn_alarm_low = std::abs(old_val) < std::abs(warnlowthresh);
        bool new_warn_alarm_low = std::abs(new_val) < std::abs(warnlowthresh);
        event_data1 = new_warn_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lnc_high) : static_cast<uint8_t>(ThresholdTypeCodes::lnc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_low != new_warn_alarm_low && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000 || (new_warn_alarm_low && first && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Warning Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_warn_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmLow");
        }
        
        bool old_alarm_high = std::abs(old_val) > std::abs(highthresh);
        bool new_alarm_high = std::abs(new_val) > std::abs(highthresh);
        event_data1 = new_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::ucr_high) : static_cast<uint8_t>(ThresholdTypeCodes::ucr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_high != new_alarm_high && tiface || (new_alarm_high && first)) {
            sendPlatformEvent(adc_name, new_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmHigh");
        }

        bool old_alarm_low = std::abs(old_val) < std::abs(lowthresh);
        bool new_alarm_low = std::abs(new_val) < std::abs(lowthresh);
        event_data1 = new_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lcr_high) : static_cast<uint8_t>(ThresholdTypeCodes::lcr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_low != new_alarm_low && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000 || (new_alarm_low && first && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Critical Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmLow");
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus exit");
}

// Schedule the next periodic update
void ADC_sensor::schedule_update() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("ADC_sensor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->update_readings();
    });
}


void ADC_sensor::sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent enter");
    [[maybe_unused]] uint16_t generatorID = 0;

    //sysgeneratorID, evmRev, sensorType, sensorNum, eventType, eventData1, eventData2, eventData3

    generatorID = (0 << 12)      // Channel
        | (0x0 << 10)             // Reserved
        | ((0 & 0x3) << 8) // Lun
        | (0x10 << 1);

    {// bit 7 of eventType: 0 = assert, 1 = deassert

        std::string sensorPath = "/voltage/" + dirName;

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