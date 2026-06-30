#include "../include/payloadtest.hpp"

bool isDBusReady() 
{
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *msg = NULL;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        return false;
    }
    
    // Try a simple ping to systemd
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "ListUnits",
                          &error,
                          &msg,
                          "");
    
    sd_bus_error_free(&error);
    if (msg) sd_bus_message_unref(msg);
    sd_bus_unref(bus);
    
    return r >= 0;
}

// Helper function to check if a systemd service is active
bool isServiceActive(const std::string& serviceName) 
{
    const int max_retries = 5;  // Increased from 3
    const int base_delay_ms = 1000;  // Longer base delay
    const int max_delay_ms = 5000;
    
    // First, wait for D-Bus to be ready
    int dbus_wait_attempts = 0;
    const int max_dbus_wait = 10;  // Wait up to ~30 seconds for D-Bus
    
    while (!isDBusReady() && dbus_wait_attempts < max_dbus_wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        dbus_wait_attempts++;
    }
    
    if (dbus_wait_attempts >= max_dbus_wait) {
        phosphor::logging::log<phosphor::logging::level::INFO>("DBus did not become ready, proceeding anyway");
    }
    
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        sd_bus *bus = NULL;
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *msg = NULL;
        char *state = NULL;
        std::string unit_path;
        int r;

        try {
            // Connect to the system bus
            r = sd_bus_open_system(&bus);
            if (r < 0) {
                throw std::runtime_error("Failed to connect to system bus: " + 
                    std::string(strerror(-r)));
            }

            // Set a timeout on the bus connection
            sd_bus_set_method_call_timeout(bus, 5000000);  // 5 second timeout

            // Use LoadUnit which is more robust
            r = sd_bus_call_method(bus,
                                  "org.freedesktop.systemd1",
                                  "/org/freedesktop/systemd1",
                                  "org.freedesktop.systemd1.Manager",
                                  "LoadUnit",
                                  &error,
                                  &msg,
                                  "s",
                                  serviceName.c_str());
            
            if (r < 0) {
                throw std::runtime_error("Failed to get unit: " + 
                    std::string(error.message ? error.message : strerror(-r)));
            }

            const char *path;
            r = sd_bus_message_read(msg, "o", &path);
            if (r < 0) {
                throw std::runtime_error("Failed to read unit path: " + 
                    std::string(strerror(-r)));
            }

            unit_path = std::string(path);
            sd_bus_message_unref(msg);
            msg = NULL;

            // Get the ActiveState property
            r = sd_bus_get_property_string(bus,
                                           "org.freedesktop.systemd1",
                                           unit_path.c_str(),
                                           "org.freedesktop.systemd1.Unit",
                                           "ActiveState",
                                           &error,
                                           &state);
            
            if (r < 0) {
                throw std::runtime_error("Failed to get ActiveState: " + 
                    std::string(error.message ? error.message : strerror(-r)));
            }

            bool isActive = (strcmp(state, "active") == 0);
            free(state);
            
            // Cleanup on success
            sd_bus_error_free(&error);
            if (msg) sd_bus_message_unref(msg);
            sd_bus_unref(bus);
            
            return isActive;
            
        } catch (const std::exception& e) {
            
            // Cleanup on failure
            sd_bus_error_free(&error);
            if (msg) sd_bus_message_unref(msg);
            if (bus) sd_bus_unref(bus);
            if (state) free(state);
            
            if (attempt < max_retries) {
                // Progressive backoff with cap
                int delay = std::min(base_delay_ms * attempt, max_delay_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
    }
    
    phosphor::logging::log<phosphor::logging::level::INFO>(("Failed to check service " + serviceName).c_str());
    
    return false;
}

namespace fs = std::filesystem;

nlohmann::json readJson(std::string path) 
{
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("PayloadTest JSON parse error: " + std::string(e.what())).c_str());
    }

    return read;
}

bool has_subdirectories(const fs::path& dir_path) 
{
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_directory()) {
            return true; // Found at least one subdirectory
        }
    }
    return false;
}

PayloadTest::PayloadTest(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref): 
    result_flag("/usr/payload_test/result_flag", std::ios::out | std::ios::trunc), status_flag("/usr/payload_test/status_flag", std::ios::out | std::ios::trunc),
    conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTest initializer");
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");
    uint8_t bus;
    uint8_t address;

    if (!result_flag.is_open() || !result_flag.good()) {
        phosphor::logging::log<phosphor::logging::level::ERR>("Failed to open result_flag");
    }
    if (!status_flag.is_open() || !status_flag.good()) {
        phosphor::logging::log<phosphor::logging::level::ERR>("Failed to open status_flag");
    }

    auto tempit = configJson.find("tempsensors");
    if (!tempit->empty()) {
        for (auto& temp_dir : configJson["tempsensors"].items()) {
            bus = static_cast<uint8_t>(configJson["tempsensors"][temp_dir.key()]["bus"]);
            address = static_cast<uint8_t>(configJson["tempsensors"][temp_dir.key()]["address"]);
            std::stringstream ss;
            ss << static_cast<int>(bus)
               << "-"
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<unsigned int>(address);
            std::string s = ss.str();
            temp_nametoaddress[temp_dir.key()] = s;
        }
    }
    
    auto adcit = configJson.find("adcsensors");
    if (!adcit->empty()) {
        for (auto& adc_dir : configJson["adcsensors"].items()) {
            bus = static_cast<uint8_t>(configJson["adcsensors"][adc_dir.key()]["bus"]);
            address = static_cast<uint8_t>(configJson["adcsensors"][adc_dir.key()]["address"]);
            std::stringstream ss;
            ss << static_cast<int>(bus)
               << "-"
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<unsigned int>(address);
            std::string s = ss.str();
            adc_nametoaddress[adc_dir.key()] = s;
        }
    }

    auto fanit = configJson.find("fancontrollers");
    if (!fanit->empty()) {
        for (auto& fan_dir : configJson["fancontrollers"].items()) {
            bus = static_cast<uint8_t>(configJson["fancontrollers"][fan_dir.key()]["bus"]);
            address = static_cast<uint8_t>(configJson["fancontrollers"][fan_dir.key()]["address"]);
            std::stringstream ss;
            ss << static_cast<int>(bus)
               << "-"
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<unsigned int>(address);
            std::string s = ss.str();
            fan_nametoaddress[fan_dir.key()] = s;
        }
    }

    auto pinit = configJson.find("gpioexpanders");
    if (!pinit->empty()) {
        for (auto& pin_dir : configJson["gpioexpanders"].items()) {
            bus = static_cast<uint8_t>(configJson["gpioexpanders"][pin_dir.key()]["bus"]);
            address = static_cast<uint8_t>(configJson["gpioexpanders"][pin_dir.key()]["address"]);
            std::stringstream ss;
            ss << static_cast<int>(bus)
               << "-"
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<unsigned int>(address);
            std::string s = ss.str();
            gpio_nametoaddress[pin_dir.key()] = s;
        }
    }

    iface_ = obj_server.add_interface("/xyz/openbmc_project/payloadTest",
        "xyz.openbmc_project.payloadTest");

    iface_->register_method("TriggerCheck",
        [this]() {
            phosphor::logging::log<phosphor::logging::level::INFO>("TriggerCheck called");
            
            runtest();
        });

    iface_->initialize();

}

PayloadTest::~PayloadTest()
{
    
}

bool PayloadTest::checkservices()
{
    if (isServiceActive("LCR_temp_sensors.service") && isServiceActive("LCR_ADC_sensors.service") && isServiceActive("LCR_fan_controller.service") && isServiceActive("LCR_fan_monitor.service") 
        && isServiceActive("LCR_Mandatory_Sensors.service") && isServiceActive("LCR_GPIO_Mon.service") && isServiceActive("phosphor-ipmi-host.service")) 
    {        
        return true;
    } else {
        return false;
    }
    return false;
}

bool PayloadTest::checktempdevices()
{
    if (!has_subdirectories("/sys/bus/i2c/drivers/lm75")) { 
        phosphor::logging::log<phosphor::logging::level::INFO>("checktempdevices no subdirectories");
        return true; 
    }
    for (const auto& entry : fs::directory_iterator("/sys/bus/i2c/drivers/lm75")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checktempdevices driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
            auto it = std::find_if(temp_nametoaddress.begin(), temp_nametoaddress.end(),
                [&path](const auto& pair) { return pair.second == path; });
    
            if (it != temp_nametoaddress.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("Value found! Associated key: " + it->first).c_str());
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    return true;
}

bool PayloadTest::checkvoltagedevices()
{
    if (!has_subdirectories("/sys/bus/i2c/drivers/ad7291")) { return true; }
    for (const auto& entry : fs::directory_iterator("/sys/bus/i2c/drivers/ad7291")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checkvoltagedevices driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
            auto it = std::find_if(adc_nametoaddress.begin(), adc_nametoaddress.end(),
                [&path](const auto& pair) { return pair.second == path; });
            if (it->second == "0-0000") {
                continue;
            }
            if (it != adc_nametoaddress.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("Value found! Associated key: " + it->first).c_str());
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    return true;
}

bool PayloadTest::checkgpiodevices()
{
    if (!has_subdirectories("/sys/bus/i2c/drivers/pca953x")) { return true; }
    for (const auto& entry : fs::directory_iterator("/sys/bus/i2c/drivers/pca953x")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checkgpiodevices driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
            auto it = std::find_if(gpio_nametoaddress.begin(), gpio_nametoaddress.end(),
                [&path](const auto& pair) { return pair.second == path; });
            if (it->second == "0-0000") {
                continue;
            }
            if (it != gpio_nametoaddress.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("Value found! Associated key: " + it->first).c_str());
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    return true;
}

bool PayloadTest::checkfandevices()
{
    if (!has_subdirectories("/sys/bus/i2c/drivers/max31790")) { return true; }
    for (const auto& entry : fs::directory_iterator("/sys/bus/i2c/drivers/max31790")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checkfandevices driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
            auto it = std::find_if(fan_nametoaddress.begin(), fan_nametoaddress.end(),
                [&path](const auto& pair) { return pair.second == path; });
    
            if (it != fan_nametoaddress.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("Value found! Associated key: " + it->first).c_str());
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    return true;
}

bool PayloadTest::checkipmb()
{
    std::vector<std::string> ipmbstocheck = {"0-0010", "1-0010"};
    std::vector<std::string> ipmbschecked;
    if (!has_subdirectories("/sys/bus/i2c/drivers/ipmb-dev")) { return false; }
    for (const auto& entry : fs::directory_iterator("/sys/bus/i2c/drivers/ipmb-dev")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checkipmb driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
    
            if (path == "0-0010" || path == "1-0010" ) {
                ipmbschecked.push_back(path);
                continue;
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    std::sort(ipmbstocheck.begin(), ipmbstocheck.end());
    std::sort(ipmbschecked.begin(), ipmbschecked.end());
    if (ipmbstocheck == ipmbschecked) {
        return true;
    }

    return false;
}

bool PayloadTest::checki2cdevices()
{
    if (checktempdevices() && checkvoltagedevices() && checkgpiodevices() && checkfandevices() && checkipmb()) {
        return true;    
    }
    return false;
}

bool PayloadTest::checkethernetphys() 
{
    std::vector<std::string> phystocheck = {"e000b000.ethernet-ffffffff:00", "e000c000.ethernet-ffffffff:00"};
    std::vector<std::string> physchecked;
    if (!has_subdirectories("/sys/bus/mdio_bus/drivers/Marvell 88E1510")) { return false; }
    for (const auto& entry : fs::directory_iterator("/sys/bus/mdio_bus/drivers/Marvell 88E1510")) {
        if (entry.is_directory()) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("checkethernetphys driver directory option " + std::string(entry.path().filename())).c_str());
            std::string path = std::string(entry.path().filename());
    
            if (path == "e000b000.ethernet-ffffffff:00" || path == "e000c000.ethernet-ffffffff:00" ) {
                physchecked.push_back(path);
                continue;
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>("Value not found");
                return false;
            }
        }
    }
    std::sort(phystocheck.begin(), phystocheck.end());
    std::sort(physchecked.begin(), physchecked.end());
    if (phystocheck == physchecked) {
        return true;
    }

    return false;
}

bool PayloadTest::checkRAM() 
{
    //check total RAM present on physical board.
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    long long totalPhysMem = memInfo.totalram;
    totalPhysMem *= memInfo.mem_unit;
    long long kBPhysMem = totalPhysMem / (1024);
    phosphor::logging::log<phosphor::logging::level::INFO>(("checkRAM Total RAM: " + std::to_string(totalPhysMem) + " B").c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("checkRAM Total RAM: " + std::to_string(totalPhysMem / (1024)) + " kB").c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("checkRAM Total RAM: " + std::to_string(totalPhysMem / (1024 * 1024)) + " MB").c_str());
    if (kBPhysMem > 900000 && kBPhysMem < 4000000) {
        return true;    
    }
    return false;
}

void PayloadTest::writeresults(bool result) 
{
    int flag = (result ? 1 : 0);
    result_flag.seekp(0);
    result_flag << flag << std::endl;
    result_flag.flush();
}

void PayloadTest::writestatus(int status) 
{
    m_status = status;
    status_flag.seekp(0);
    status_flag << status << std::endl;
    status_flag.flush();
}

void PayloadTest::runtest()
{
    if (m_status != 1) {
        phosphor::logging::log<phosphor::logging::level::INFO>("runtest: Payload Test running now.");
        writestatus(1);
        writeresults(0); //write 0 before the results have actually been retrieved
        bool i2cdevices = checki2cdevices();
        bool RAM = checkRAM();
        bool phys = checkethernetphys();
        if (!phys){
            std::this_thread::sleep_for(std::chrono::milliseconds(15000));
            phys = checkethernetphys();
        }
        bool services = checkservices();
        if (!services){
            std::this_thread::sleep_for(std::chrono::milliseconds(30000));
            services = checkservices();
        }
        writeresults(RAM && services && i2cdevices && phys);
        writestatus(0);
    } else {
        phosphor::logging::log<phosphor::logging::level::INFO>("runtest: Payload Test is already running.");
    }
} 
