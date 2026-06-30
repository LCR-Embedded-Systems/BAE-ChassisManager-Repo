#include "../include/MANAGER.hpp"

std::ofstream boot_dbgfile("boot_lcr.dbg.log", std::ios::app);
std::ofstream system_logs("system_logs.log", std::ios::app);

const size_t MAX_SIZE = 8 * 1024;  // 8 kB in bytes

// Check if D-Bus system is ready
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

nlohmann::json Manager::readJson(std::string path) 
{
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

void Manager::init_system_logs() 
{

    //read the old logfile from flash, and load that into the logfile in the filesystem.

    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: attempting to read system logs from flash" << std::endl;

    offset = 0;
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
                    offset = fileo.tellg();
                    outFile << lineo << '\n';
                } else {
                    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: first character of strange line: " << static_cast<unsigned char>(lineo[0]) << std::endl;    
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

    std::string newlog = "[" + timeSinceBoot() + "] " + "LCR: " + log + "\n";

    system_logs << "[" << timeSinceBoot() << "] " << "LCR: " << log << std::endl;

    newlog_string += newlog;

    trim_log();

}

void Manager::write_logs_to_flash() 
{

    int fd = open("/dev/mtd4", O_RDWR | O_SYNC);
    if (fd < 0) {
        boot_dbgfile << "Failed to open /dev/mtd4: " << strerror(errno) << std::endl;
        return;
    }

    mtd_info_t mtd_info;
    if (ioctl(fd, MEMGETINFO, &mtd_info) != 0) {
        boot_dbgfile << "Failed to get MTD info: " << strerror(errno) << std::endl;
        close(fd);
        return;
    }

    if (!(mtd_info.flags & MTD_WRITEABLE)) {
        boot_dbgfile << "MTD is not writable" << std::endl;
        close(fd);
        return;
    }

    size_t erase_size = mtd_info.erasesize;
    size_t data_size = newlog_string.size();
    off_t start_offset = offset;
    off_t end_offset = start_offset + data_size - 1;

    if (end_offset >= static_cast<off_t>(mtd_info.size)) {
        boot_dbgfile << "Data would exceed device size" << std::endl;
        close(fd);
        return;
    }

    // Calculate blocks to erase
    off_t start_block = start_offset & ~(erase_size - 1);
    off_t end_block = end_offset & ~(erase_size - 1);

    for (off_t block = start_block; block <= end_block; block += erase_size) {
        // Read the block to check its contents
        std::vector<uint8_t> buffer(erase_size);
        lseek(fd, block, SEEK_SET);
        ssize_t read_bytes = read(fd, buffer.data(), erase_size);
        if (read_bytes != static_cast<ssize_t>(erase_size)) {
            boot_dbgfile << "Failed to read block at " << block << ": " << strerror(errno) << std::endl;
            close(fd);
            return;
        }

        // Check if the block has any characters other than 0xFF (i.e., has data)
        bool has_other_than_ff = false;
        for (const auto& b : buffer) {
            if (b != 0xFF) {
                has_other_than_ff = true;
                break;
            }
        }

        // Do not erase if it has characters other than 0xFF
        if (has_other_than_ff) {
            continue;
        }

        erase_info_t erase;
        erase.start = block;
        erase.length = erase_size;

        if (ioctl(fd, MEMUNLOCK, &erase) != 0) {
            boot_dbgfile << "Unlock failed for block at " << block << ": " << strerror(errno) << std::endl;
            if (errno != ENOTSUP) {
                close(fd);
                return;
            }
        }

        if (ioctl(fd, MEMERASE, &erase) != 0) {
            boot_dbgfile << "Erase failed for block at " << block << ": " << strerror(errno) << std::endl;
            close(fd);
            return;
        }
    }

    lseek(fd, start_offset, SEEK_SET);
    ssize_t written = write(fd, newlog_string.data(), data_size);
    if (written < 0) {
        boot_dbgfile << "Write failed: " << strerror(errno) << " (errno: " << errno << ")" << std::endl;
    } else if (written != static_cast<ssize_t>(data_size)) {
        boot_dbgfile << "Write incomplete: wrote " << written << " of " << data_size << " bytes" << std::endl;
    } else {
        update_last_append_offset(start_offset + data_size);
    }

    close(fd);
    newlog_string = "";
}

void Manager::update_last_append_offset(off_t new_offset) 
{
    offset = new_offset;
}

void Manager::trim_log() 
{

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

void Manager::write_heartbeat() 
{
    update_readings(1);
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
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    ADCpaths paths;

    std::string stub = "/sys/bus/iio/devices/";
    std::string directory;

    for (auto& adc_dir : configJson["adcsensors"].items()) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor directory " + adc_dir.key()).c_str());
        boot_dbgfile << "[" << currentTimestamp() << "] " << adc_dir.key() << " bus: " << configJson["adcsensors"][adc_dir.key()]["bus"] << std::endl;
        if (configJson["adcsensors"][adc_dir.key()]["bus"] == 0 && configJson["adcsensors"][adc_dir.key()]["address"] == 0) { // bus and address is 0, this is the vpx bus
            if (!std::filesystem::is_directory(stub + adc_dir.key())) {
                stub += "iio:device0";
                directory = adc_dir.key();
                boot_dbgfile << "[" << currentTimestamp() << "] " << adc_dir.key() << " does not exist, using default" << std::endl;
                break;
            }
            stub += adc_dir.key();
            directory = adc_dir.key();
            boot_dbgfile << "[" << currentTimestamp() << "] " << adc_dir.key() << " bus and address are 0" << std::endl;
        }
    }

    paths.V12path     = stub + "/in_voltage9_raw";
    paths.V3_3path    = stub + "/in_voltage10_raw";
    paths.V5path      = stub + "/in_voltage11_raw";
    paths.V3_3Auxpath = stub + "/in_voltage12_raw";
    paths.Vp12Auxpath = stub + "/in_voltage13_raw";
    paths.Vn12Auxpath = stub + "/in_voltage14_raw";

    fancontrol = configJson["fancontrol"];

    temp_service_status = false;
    voltage_service_status = false;
    fan_controller_service_status = false;
    mandatory_sensor_service_status = false;
    gpio_service_status = false;
    ipmi_service_status = false;

    init_system_logs();

    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");
    write_log_to_file("CHASSIS MANAGER BOOTUP");
    write_log_to_file("Chassis name detected: " + configJson["name"].get<std::string>());
    write_log_to_file("FPGA has finished boot. Kernel booting has begun. These logs are for the BOOT SEQUENCE AND MANAGER application.");
    write_log_to_file("This application manages the System Reset and PS inhibit signals.");
    write_log_to_file("Once voltages are nominal and services are active, the system reset is released, allowing the cards to boot.");
    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");

    limitJson = readJson("/usr/share/thresholds/thresholds.json");

    
    ADCs.push_back(std::make_unique<ADC_element>("12V_Rail",     paths.V12path,     12.0,  configJson["adcsensors"][directory.c_str()]["12V_Rail"]["present"]));
    ADCs.push_back(std::make_unique<ADC_element>("3_3V_Rail",    paths.V3_3path,    3.3,   configJson["adcsensors"][directory.c_str()]["3_3V_Rail"]["present"]));
    ADCs.push_back(std::make_unique<ADC_element>("5V_Rail",      paths.V5path,      5.0,   configJson["adcsensors"][directory.c_str()]["5V_Rail"]["present"]));
    ADCs.push_back(std::make_unique<ADC_element>("3_3auxV_Rail", paths.V3_3Auxpath, 3.3,   configJson["adcsensors"][directory.c_str()]["3_3auxV_Rail"]["present"]));
    ADCs.push_back(std::make_unique<ADC_element>("12pauxV_Rail", paths.Vp12Auxpath, 12.0,  configJson["adcsensors"][directory.c_str()]["12pauxV_Rail"]["present"]));
    ADCs.push_back(std::make_unique<ADC_element>("12nauxV_Rail", paths.Vn12Auxpath, -12.0, configJson["adcsensors"][directory.c_str()]["12nauxV_Rail"]["present"]));

    calculate_cs();

    sysreset.input=false;
    sysreset.chip=1;
    sysreset.line=0;
    sysreset.status=0;
    sysreset.dbusflag=false;
    sysreset.name="sysreset";

    ps_enable.input=false;
    ps_enable.chip=3;
    ps_enable.line=3;
    ps_enable.status=0;
    ps_enable.dbusflag=false;
    ps_enable.name="psenable";

    ps_inhibit1.input=false;
    ps_inhibit1.chip=3;
    ps_inhibit1.line=2;
    ps_inhibit1.status=0;
    ps_inhibit1.dbusflag=false;
    ps_inhibit1.name="ps1inh";

    ps_inhibit2.input=false;
    ps_inhibit2.chip=3;
    ps_inhibit2.line=1;
    ps_inhibit2.status=0;
    ps_inhibit2.dbusflag=false;
    ps_inhibit2.name="ps2inh";

    switch_gpio.input=true;
    switch_gpio.chip=0;
    switch_gpio.line=0;
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

void Manager::calculate_cs() 
{
    for (const auto& ADC : ADCs) {
        ADC->scale_coeff = (ADC->V_target / 2047);
    }
}

bool Manager::getfancontrol()
{
    return fancontrol;
}

bool Manager::wait_for_switch() 
{
    watch_services();
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

        line.release();
        return false;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::set_alarm_gpio() 
{
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

bool Manager::update_readings(int num) 
{

    bool bootflag = false;

    for (const auto& ADC : ADCs) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: ADC path: " << ADC->path << std::endl;
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: reading value: " << readFile(ADC->path) << std::endl;
        if (ADC->present) {
            ADC->raw_reading = std::stoi(readFile(ADC->path));
            ADC->V_current = ADC->scale_coeff * ADC->raw_reading;
            double lowthresh = limitJson[ADC->name]["critmin"];
            double highthresh = limitJson[ADC->name]["critmax"];
            if (std::abs(ADC->V_current) > std::abs(lowthresh) && std::abs(ADC->V_current) < std::abs(highthresh)) {
                // boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is safe at reading " << ADC->V_current << std::endl;
                ADC->safe = true;
            } else {
                boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is not safe at reading " << ADC->V_current << std::endl;
                if (num % 15 == 0) {
                    write_log_to_file("Voltage monitoring: " + ADC->name + " is unsafe at reading " + std::to_string(ADC->V_current));
                }
                ADC->safe = false;
                bootflag = false;
                return bootflag;
            }
        }
    }

    for (const auto& ADC : ADCs) {
        if (ADC->present) {
            if (ADC->safe == false) {
                boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is not safe at reading " << ADC->V_current << std::endl;
                bootflag = false;
                return bootflag;
            }
        }
    }

    bootflag = true;
    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: bootflag is true" << std::endl;
    
    return bootflag;
}

ADC_element* Manager::getSensorByName(const std::string& name) 
{
    for (const auto& ADC : ADCs) {
        if (ADC->name == name) {
            return ADC.get();
        }
    }
    return nullptr;
}

bool Manager::set_ps() 
{
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

bool Manager::set_ps_inh1() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_inhibit1.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_inhibit1.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting ps inh1" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_inhibit1.status=1;
        write_log_to_file("Setting Power Supply inhibit 1 pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set ps inh1" << std::endl;
        line.release();

        ps_inhibit1.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::set_ps_inh2() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_inhibit2.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_inhibit2.line);
    
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: setting ps inh2" << std::endl;
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_inhibit2.status=1;
        write_log_to_file("Setting Power Supply inhibit 2 pin");
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: exiting set ps inh2" << std::endl;
        line.release();

        ps_inhibit2.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error in GPIO operations: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::unset_ps() 
{
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

bool Manager::get_fan_controller_status() 
{
    watch_services();
    return fan_controller_service_status;
}

bool Manager::set_sr() 
{
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

bool Manager::check_temp_service()
{
    return isServiceActive("LCR_temp_sensors.service");
}

bool Manager::check_voltage_service()
{
    return isServiceActive("LCR_ADC_sensors.service");
}

bool Manager::check_fan_controller_service()
{
    return isServiceActive("LCR_fan_controller.service");
}

bool Manager::check_mandatory_sensor_service()
{
    return isServiceActive("LCR_Mandatory_Sensors.service");
}

bool Manager::check_gpio_service()
{
    return isServiceActive("LCR_GPIO_Mon.service");    
}

bool Manager::check_ipmi_host_service()
{
    return isServiceActive("phosphor-ipmi-host.service");
}

void Manager::log_servicechange(std::string* log, bool status) 
{
    if (status == false) {
        *log += " to OFF";
    } else {
        *log += " to ON";
    }
}

void Manager::watch_services() 
{
    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: watching service statuses" << std::endl;
    bool temp_temp_status = check_temp_service();
    bool temp_voltage_status = check_voltage_service();
    bool temp_fan_controller_status = check_fan_controller_service();
    bool temp_mandatory_sensor_status = check_mandatory_sensor_service();
    bool temp_gpio_status = check_gpio_service();
    bool temp_ipmi_status = check_ipmi_host_service();

    ipmi_service_status = temp_ipmi_status;

    if (temp_temp_status != temp_service_status) {
        std::string log;
        log += "Temperature monitoring service status changed";
        log_servicechange(&log, temp_temp_status);
        temp_service_status = temp_temp_status;
        write_log_to_file(log);
    }
    if (temp_voltage_status != voltage_service_status) {
        std::string log;
        log += "Voltage monitoring service status changed";
        log_servicechange(&log, temp_voltage_status);
        voltage_service_status = temp_voltage_status;
        write_log_to_file(log);
    }
    if (temp_fan_controller_status != fan_controller_service_status) {
        std::string log;
        log += "Fan Controller service status changed";
        log_servicechange(&log, temp_fan_controller_status);
        fan_controller_service_status = temp_fan_controller_status;
        write_log_to_file(log);
    }
    if (temp_mandatory_sensor_status != mandatory_sensor_service_status) {
        std::string log;
        log += "Mandatory Sensors service status changed";
        log_servicechange(&log, temp_mandatory_sensor_status);
        mandatory_sensor_service_status = temp_mandatory_sensor_status;
        write_log_to_file(log);
    }
    if (temp_gpio_status != gpio_service_status) {
        std::string log;
        log += "GPIO monitoring service status changed";
        log_servicechange(&log, temp_gpio_status);
        gpio_service_status = temp_gpio_status;
        write_log_to_file(log);
    }
    if (temp_ipmi_status != ipmi_service_status) {
        std::string log;
        log += "IPMI host service status changed";
        log_servicechange(&log, temp_ipmi_status);
        ipmi_service_status = temp_ipmi_status;
        write_log_to_file(log);
    }

}

void Manager::watch_gpios() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string property = "Value";
    while(true){
        if (gpio_service_status) {

            auto bus = sdbusplus::bus::new_default();

            int ps_value = ps_enable.status;
            int inh1_value = ps_inhibit1.status;
            int inh2_value = ps_inhibit2.status;
            int sr_value = sysreset.status;
    
            std::string ps_name = ps_enable.name;
            std::string ps_path = "/xyz/openbmc_project/gpio/" + ps_name;

            std::string psinh1_name = ps_inhibit1.name;
            std::string psinh1_path = "/xyz/openbmc_project/gpio/" + psinh1_name;

            std::string psinh2_name = ps_inhibit2.name;
            std::string psinh2_path = "/xyz/openbmc_project/gpio/" + psinh2_name;
    
            std::string sr_name = sysreset.name;
            std::string sr_path = "/xyz/openbmc_project/gpio/" + sr_name;
    
            try {
                if (ps_enable.dbusflag){
                    phosphor::interface::util::setProperty<int>(bus, ps_path, interface, property, std::move(ps_value));
                    ps_enable.dbusflag=false;
                }
                if (ps_inhibit1.dbusflag){
                    phosphor::interface::util::setProperty<int>(bus, psinh1_path, interface, property, std::move(inh1_value));
                    ps_inhibit1.dbusflag=false;
                }
                if (ps_inhibit2.dbusflag){
                    phosphor::interface::util::setProperty<int>(bus, psinh2_path, interface, property, std::move(inh2_value));
                    ps_inhibit2.dbusflag=false;
                }
            } catch (const std::exception& e) {
                boot_dbgfile << "[" << currentTimestamp() << "] " << "Failed to set property for " << ps_name << ": " << e.what() << std::endl;
            }
            
            try {
                if (sysreset.dbusflag){
                    phosphor::interface::util::setProperty<int>(bus, sr_path, interface, property, std::move(sr_value));
                    sysreset.dbusflag = false;
                }
            } catch (const std::exception& e) {
                boot_dbgfile << "[" << currentTimestamp() << "] " << "Failed to set property for " << sr_name << ": " << e.what() << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Manager::watch_alarms() 
{
    
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
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: checking temperature alarms" << std::endl;
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
                    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting temperature alarm properties for " << path << ": " << e.what() << std::endl;
                    // Continue to next path, treating this sensor as no alarm
                }
            }
        } catch (const std::exception& e) {
            boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: Error getting temperature subtree paths: " << e.what() << std::endl;
            // Treat as no alarm if subtree fetch fails
        }
        
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: checking voltage alarms" << std::endl;
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
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: temperature alarm: " << (temp_alarm ? "ON" : "OFF") << std::endl;
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: voltage alarm: " << (voltage_alarm ? "ON" : "OFF") << std::endl;

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