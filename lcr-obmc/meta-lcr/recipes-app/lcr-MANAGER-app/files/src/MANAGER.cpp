#include "../include/MANAGER.hpp"

//attempted test change

std::ofstream boot_dbgfile("boot_lcr.dbg.log", std::ios::app);
std::ofstream system_logs("system_logs.log", std::ios::app);

const size_t MAX_SIZE = 1024 * 1024;  // 1 MB in bytes


struct ADCpaths
{
    std::string V12path = "/sys/bus/iio/devices/iio:device0/in_voltage9_raw";
    std::string V3_3path = "/sys/bus/iio/devices/iio:device0/in_voltage10_raw";
    std::string V5path = "/sys/bus/iio/devices/iio:device0/in_voltage11_raw";
    std::string V3_3Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage12_raw";
    std::string Vp12Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage13_raw";
    std::string Vn12Auxpath = "/sys/bus/iio/devices/iio:device0/in_voltage14_raw";
};

// Check if D-Bus system is ready
bool isDBusReady() {
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
bool isServiceActive(const std::string& serviceName) {
    const int max_retries = 5;  // Increased from 3
    const int base_delay_ms = 1000;  // Longer base delay
    const int max_delay_ms = 5000;
    
    // First, wait for D-Bus to be ready
    int dbus_wait_attempts = 0;
    const int max_dbus_wait = 10;  // Wait up to ~30 seconds for D-Bus
    
    while (!isDBusReady() && dbus_wait_attempts < max_dbus_wait) {
        boot_dbgfile << "[" << currentTimestamp() << "] " 
                     << "Waiting for D-Bus to become ready..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        dbus_wait_attempts++;
    }
    
    if (dbus_wait_attempts >= max_dbus_wait) {
        boot_dbgfile << "[" << currentTimestamp() << "] " 
                     << "D-Bus did not become ready, proceeding anyway" << std::endl;
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
            // Log error
            boot_dbgfile << "[" << currentTimestamp() << "] " 
                        << "Attempt " << attempt << " error checking service " 
                        << serviceName << ": " << e.what() << std::endl;
            
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
    
    boot_dbgfile << "[" << currentTimestamp() << "] " 
                 << "Failed to check service " << serviceName 
                 << " after " << max_retries << " attempts" << std::endl;
    
    return false;
}

nlohmann::json Manager::readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

void Manager::init_system_logs() {

    //read the old logfile from flash, and load that into the logfile in the filesystem.

    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: attempting to read system logs from flash" << std::endl;

    std::string filename = "/dev/mtd4";
    std::ifstream fileo(filename);

    std::ofstream outFile("system_logs.log");
    std::string lineo;

    int linenum = 1;

    while (std::getline(fileo, lineo)) {
        size_t lineLength = lineo.length();
        // Write line to output file with a newline
        if (lineLength < 1024) {
            if (!lineo.empty()){
                if (lineo[0] == '['){
                    outFile << lineo << '\n';
                } else {
                    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: first character of strange line: " 
                        << static_cast<unsigned char>(lineo[0]) << std::endl;    
                }
            } else {
                boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: skipping empty line" << std::endl;
            }
        } else {
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: long line length: " << lineLength << std::endl;
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: long line first character: " 
                         << static_cast<unsigned char>(lineo[0]) << std::endl;
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Line number: " << linenum << std::endl;
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: finished reading logs from flash" << std::endl;
            break;
        }
        linenum = linenum + 1;
    }
    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: read entire qspi-logs flash section and wrote all of it to system logs" << std::endl;
}

void Manager::write_log_to_file(std::string log) {

    system_logs << "[" << timeSinceBoot() << "] " << "LCR: " << log << std::endl;

    trim_log();

}

void Manager::trim_log() {

    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: trimming log now..." << std::endl;

    std::string filename = "system_logs.log";

    try {
        size_t current_size = fs::file_size(filename);
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: current_size: " << current_size << std::endl;
        if (current_size <= MAX_SIZE) return;

        std::ifstream in(filename, std::ios::binary);
        if (!in) return;

        size_t to_skip = current_size - MAX_SIZE;
        size_t skipped = 0;
        char ch;
        while (in.get(ch)) {
            ++skipped;
            if (ch == '\n' && skipped >= to_skip) {
                break;  // Stop after a full line when enough bytes are skipped
            }
        }

        if (!in) return;  // Reached EOF without skipping enough (unlikely)

        // Now 'in' is positioned at the start of the first log to keep
        std::string temp_filename = filename + ".tmp";
        std::ofstream out(temp_filename, std::ios::binary);
        if (!out) return;

        out << in.rdbuf();  // Efficiently copy the remaining content

        in.close();
        out.close();

        fs::remove(filename);
        fs::rename(temp_filename, filename);
        // Close and reopen the global system_logs stream
        system_logs.close();
        system_logs.open("system_logs.log", std::ios::app);
    } catch (const fs::filesystem_error& e) {
        boot_dbgfile << "Filesystem error: " << e.what() << std::endl;
    }

}

void Manager::write_heartbeat() {
    update_readings();
    std::string ADClog;
    ADClog = "System heartbeat... ";
    for (const auto& ADC : ADCs) {
        ADClog += ADC->name;
        ADClog += std::string(": ");
        ADClog += std::to_string(ADC->V_current);
        ADClog += std::string("V ");
    }
    write_log_to_file(ADClog);
}

Manager::Manager()
{
    temp_service_status = false;
    voltage_service_status = false;
    fan_controller_service_status = false;
    mandatory_sensor_service_status = false;
    gpio_service_status = false;
    ipmi_service_status = false;

    init_system_logs();

    write_log_to_file("Initializing system logs...");
    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");
    write_log_to_file("CHASSIS MANAGER BOOTUP");
    write_log_to_file("BOOTUP LOGS BEGIN HERE");
    write_log_to_file("FPGA has finished boot. Kernel booting has begun. These logs are for the BOOT SEQUENCE AND MANAGER application.");
    write_log_to_file("This application manages the System Reset and PS enable signals.");
    write_log_to_file("Once voltages are nominal and services are active, the system reset is released, allowing the cards to boot.");
    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");

    limitJson = readJson("/usr/share/thresholds/thresholds.json");

    ADCpaths ADCpaths;
    
    ADCs.push_back(std::make_unique<ADC_element>("12V_Rail", ADCpaths.V12path, 12.0));
    ADCs.push_back(std::make_unique<ADC_element>("3_3V_Rail", ADCpaths.V3_3path, 3.3));
    ADCs.push_back(std::make_unique<ADC_element>("5V_Rail", ADCpaths.V5path, 5.0));
    ADCs.push_back(std::make_unique<ADC_element>("3_3auxV_Rail", ADCpaths.V3_3Auxpath, 3.3));
    ADCs.push_back(std::make_unique<ADC_element>("12pauxV_Rail", ADCpaths.Vp12Auxpath, 12.0));
    ADCs.push_back(std::make_unique<ADC_element>("12nauxV_Rail", ADCpaths.Vn12Auxpath, 12.0));

    calculate_cs();

    sysreset.input=false;
    sysreset.chip=1;
    sysreset.line=0;
    sysreset.status=0;
    sysreset.dbusflag=false;
    sysreset.name="sysreset";

    ps_enable.input=false;
    ps_enable.chip=2;
    ps_enable.line=3;
    ps_enable.status=0;
    ps_enable.dbusflag=false;
    ps_enable.name="psenable";

    switch_gpio.input=true;
    switch_gpio.chip=0;
    switch_gpio.line=6;
    switch_gpio.status=0;
    switch_gpio.name="switch_gpio";

    alarm_gpio.input=false;
    alarm_gpio.chip=0;
    alarm_gpio.line=11;
    alarm_gpio.status=0;
    alarm_gpio.name="alarm_gpio";
}

Manager::~Manager()
{
    ;
}

void Manager::calculate_cs() {
    for (const auto& ADC : ADCs) {
        ADC->scale_coeff = (ADC->V_target / 2047);
    }
}

bool Manager::wait_for_switch() {
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(switch_gpio.chip));

        // Get the line
        gpiod::line line = chip.get_line(switch_gpio.line);
        
        // Request line as input
        line.request({"readgpio", gpiod::line_request::DIRECTION_INPUT, 0});
        
        // Read the current value
        int value = line.get_value();
        
        // Update status
        switch_gpio.status = value;

        if (value == 1) {
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: switch was turned on" << std::endl;
            line.release();
            write_log_to_file("Front Panel switch was turned on...");
            return true;
        }

        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: switch is currently off" << std::endl;

        line.release();
        return false;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::set_alarm_gpio() {
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(switch_gpio.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(switch_gpio.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting alarm" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        switch_gpio.status=1;
        write_log_to_file("Setting Alarm gpio pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set alarm" << std::endl;
        line.release();

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::update_readings() {

    bool bootflag = false;

    for (const auto& ADC : ADCs) {
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: reading value: ";
        ADC->raw_reading = std::stoi(readFile(ADC->path));
        ADC->V_current = ADC->scale_coeff * ADC->raw_reading;
        double lowthresh = limitJson[ADC->name]["min"];
        double highthresh = limitJson[ADC->name]["max"];
        if (ADC->V_current > lowthresh && ADC->V_current < highthresh) {
            // boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is safe at reading " << ADC->V_current << std::endl;
            ADC->safe = true;
        } else {
            boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is not safe at reading " << ADC->V_current << std::endl;
            write_log_to_file("Voltage monitoring: " + ADC->name + " is unsafe at reading " + std::to_string(ADC->V_current));
            ADC->safe = false;
            bootflag = false;
            return bootflag;
        }
    }

    for (const auto& ADC : ADCs) {
        if (ADC->safe == false) {
            boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is not safe at reading " << ADC->V_current << std::endl;
            bootflag = false;
            return bootflag;
        }
    }

    bootflag = true;
    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: bootflag is true" << std::endl;
    
    
    return bootflag;
}

ADC_element* Manager::getSensorByName(const std::string& name) {
    for (const auto& ADC : ADCs) {
        if (ADC->name == name) {
            return ADC.get();
        }
    }
    return nullptr;
}

bool Manager::set_ps() {
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_enable.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_enable.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting ps" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_enable.status=1;
        write_log_to_file("Setting Power Supply Enable pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set ps" << std::endl;
        line.release();

        ps_enable.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::unset_ps() {
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_enable.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_enable.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting ps" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
        ps_enable.status=0;
        write_log_to_file("Unsetting Power Supply Enable pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set ps" << std::endl;
        line.release();

        ps_enable.dbusflag=false;

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::get_fan_controller_status() {
    watch_services();
    return fan_controller_service_status;
}

bool Manager::set_sr() {
    write_log_to_file("Voltage monitoring: voltages within range, driving system reset pin to begin chassis boot");
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(sysreset.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(sysreset.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting sysreset, adjust with better hardware" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
        sysreset.status=1;
        write_log_to_file("Setting System Reset pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set sysreset" << std::endl;
        line.release();

        sysreset.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

void Manager::write_logs_to_flash() {
    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: writing logs to qspi flash" << std::endl;
    system("flashcp /system_logs.log /dev/mtd4");
}

bool Manager::check_temp_service(){
    return isServiceActive("LCR_temp_sensors.service");
}

bool Manager::check_voltage_service(){
    return isServiceActive("LCR_ADC_sensors.service");
}

bool Manager::check_fan_controller_service(){
    return isServiceActive("LCR_fan_controller.service");
}

bool Manager::check_mandatory_sensor_service(){
    return isServiceActive("LCR_Mandatory_Sensors.service");
}

bool Manager::check_gpio_service(){
    return isServiceActive("LCR_GPIO_Mon.service");    
}

bool Manager::check_ipmi_host_service(){
    return isServiceActive("phosphor-ipmi-host.service");
}

void Manager::log_servicechange(bool status) {
    if (status == false) {
        system_logs << " to OFF" << std::endl;
    } else {
        system_logs << " to ON" << std::endl;
    }
}

void Manager::watch_services() {
    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: watching service statuses" << std::endl;
    bool temp_temp_status = check_temp_service();
    bool temp_voltage_status = check_voltage_service();
    bool temp_fan_controller_status = check_fan_controller_service();
    bool temp_mandatory_sensor_status = check_mandatory_sensor_service();
    bool temp_gpio_status = check_gpio_service();
    bool temp_ipmi_status = check_ipmi_host_service();

    ipmi_service_status = temp_ipmi_status;

    if (temp_temp_status != temp_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: Temperature monitoring service status changed";
        log_servicechange(temp_temp_status);
        temp_service_status = temp_temp_status;
    }
    if (temp_voltage_status != voltage_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: Voltage monitoring service status changed";
        log_servicechange(temp_voltage_status);
        voltage_service_status = temp_voltage_status;
    }
    if (temp_fan_controller_status != fan_controller_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: Fan Controller service status changed";
        log_servicechange(temp_fan_controller_status);
        fan_controller_service_status = temp_fan_controller_status;
    }
    if (temp_mandatory_sensor_status != mandatory_sensor_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: Mandatory Sensors service status changed";
        log_servicechange(temp_mandatory_sensor_status);
        mandatory_sensor_service_status = temp_mandatory_sensor_status;
    }
    if (temp_gpio_status != gpio_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: GPIO monitoring service status changed";
        log_servicechange(temp_gpio_status);
        gpio_service_status = temp_gpio_status;
    }
    if (temp_ipmi_status != ipmi_service_status) {
        system_logs << "[" << timeSinceBoot() << "] " << "LCR: IPMI host service status changed";
        log_servicechange(temp_ipmi_status);
        ipmi_service_status = temp_ipmi_status;
    }

}

void Manager::watch_gpios() {
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string property = "Value";
    while(true){
        if (gpio_service_status) {

            auto bus = sdbusplus::bus::new_default();

            int ps_value = ps_enable.status;
            int sr_value = sysreset.status;
    
            std::string ps_name = ps_enable.name;
            std::string ps_path = "/xyz/openbmc_project/gpio/" + ps_name;
    
            std::string sr_name = sysreset.name;
            std::string sr_path = "/xyz/openbmc_project/gpio/" + sr_name;
    
            try {
                if (ps_enable.dbusflag){
                    phosphor::interface::util::setProperty<double>(bus, ps_path, interface, property, std::move((double)ps_value));
                    ps_enable.dbusflag=false;
                }
            } catch (const std::exception& e) {
                boot_dbgfile << "[" << currentTimestamp() << "] " << "Failed to set property for " << ps_name << ": " << e.what() << std::endl;
            }
            
            try {
                if (sysreset.dbusflag){
                    phosphor::interface::util::setProperty<double>(bus, sr_path, interface, property, std::move((double)sr_value));
                    sysreset.dbusflag = false;
                }
            } catch (const std::exception& e) {
                boot_dbgfile << "[" << currentTimestamp() << "] " << "Failed to set property for " << sr_name << ": " << e.what() << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Manager::watch_alarms() {
    
    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: watching alarms..." << std::endl;
    while(ipmi_service_status == false) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: ipmi host is not yet enabled..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        watch_services();
    }

    auto bus = sdbusplus::bus::new_default();

    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string value_interface = "xyz.openbmc_project.Sensor.Value";
    std::string voltage_service = "xyz.openbmc_project.ADCHwmon";
    std::string voltage_stub = "/xyz/openbmc_project/sensors/voltage";
    std::string temperature_service = "xyz.openbmc_project.temperatureHwmon";
    std::string temperature_stub = "/xyz/openbmc_project/sensors/temperature";
    std::string pwm_stub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tach_stub = "/xyz/openbmc_project/sensors/fan_tach";

    bool temp_alarm;
    bool voltage_alarm;
    bool fan_alarm;
    bool alarms = false;
    double voltage_reading;
    double temperature_reading;
    std::string temp_name;
    std::string voltage_name;
    while(alarms == false) {
        watch_services();
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: checking temperature alarms" << std::endl;
        temp_alarm = false;
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            auto temp_paths = phosphor::interface::util::getSubTreePathsRaw(bus, temperature_stub, threshold_interface, 0);
            for (auto& path : temp_paths) {
                try {
                    bool critical_low = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow"));
                    bool critical_high = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"));
                    temp_alarm = (critical_low || critical_high) ? true : false;
                    if (temp_alarm) {
                        temperature_reading = phosphor::interface::util::getProperty<double>(bus, path, value_interface, std::string("Value"));
                        temp_name = path;
                        break;
                    }
                } catch (const std::exception& e) {
                    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting temperature alarm properties for " << path << ": " << e.what() << std::endl;
                    // Continue to next path, treating this sensor as no alarm
                }
            }
        } catch (const std::exception& e) {
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting temperature subtree paths: " << e.what() << std::endl;
            // Treat as no alarm if subtree fetch fails
        }
        
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: checking voltage alarms" << std::endl;
        voltage_alarm = false;
        try {
            auto voltage_paths = phosphor::interface::util::getSubTreePathsRaw(bus, voltage_stub, threshold_interface, 0);
            for (auto& path : voltage_paths) {
                try {
                    bool critical_low = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow"));
                    bool critical_high = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"));
                    voltage_alarm = (critical_low || critical_high) ? true : false;
                    if (voltage_alarm) {
                        voltage_reading = phosphor::interface::util::getProperty<double>(bus, path, value_interface, std::string("Value"));
                        voltage_name = path;
                        break;
                    }
                } catch (const std::exception& e) {
                    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting voltage alarm properties for " << path << ": " << e.what() << std::endl;
                    // Continue to next path, treating this sensor as no alarm
                }
            }
        } catch (const std::exception& e) {
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting voltage subtree paths: " << e.what() << std::endl;
            // Treat as no alarm if subtree fetch fails
        }
        alarms = (temp_alarm || voltage_alarm) ? true : false;
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: temperature alarm: " << (temp_alarm ? "ON" : "OFF") << std::endl;
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: voltage alarm: " << (voltage_alarm ? "ON" : "OFF") << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    }
    system_logs << "[" << timeSinceBoot() << "] " << "LCR: An alarm is found to be high" << std::endl;
    system_logs << "[" << timeSinceBoot() << "] " << "LCR: Alarm: " << std::endl;
    if (temp_alarm) {
        write_log_to_file("Temperature Alarm at path " + temp_name + " with reading " + std::to_string(temperature_reading));
    } else if (voltage_alarm) {
        write_log_to_file("Voltage Alarm at path " + voltage_name + " with reading " + std::to_string(voltage_reading));
    }
    unset_ps();
    set_alarm_gpio();
    write_log_to_file("Alarm is set, reset the chassis.");
    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: alarms are high" << std::endl;
    
}