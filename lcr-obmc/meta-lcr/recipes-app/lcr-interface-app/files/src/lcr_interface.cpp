#include "../include/lcr_interface.hpp"

const int name_width     = 25;
const int value_width    = 12;
const int crit_thresh_width   = 18;
const int warn_thresh_width   = 18;
const int crit_alarm_width    = 18;
const int warn_alarm_width    = 18;

using json = nlohmann::json;

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
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        dbus_wait_attempts++;
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
    
    return false;
}

CLI::CLI() : bus(sdbusplus::bus::new_default()) {};

CLI::~CLI() {;};

int CLI::process_argument(int nargs, char* args[]) 
{

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
            printsplash();display_temps();
        } else if (arg == "voltage" || arg == "voltages") {
            printsplash();display_ADC();
        } else if (arg == "current" || arg == "currents") {
            printsplash();display_current();
        } else if (arg == "power" || arg == "powers") {
            printsplash();display_power();
        } else if (arg == "fan" || arg == "fans") {
            display_fans();
        } else if (arg == "service" || arg == "services") {
            display_services();
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

void CLI::printsplash()
{
    std::cout << std::left
              << std::setw(name_width) << "Name"
              << std::right
              << std::setw(value_width) << "Value"
              << std::right
              << std::setw(crit_thresh_width) << "Min Crit Thresh"
              << std::right
              << std::setw(crit_alarm_width) << "Low Crit Alarm"
              << std::right
              << std::setw(crit_thresh_width) << "Max Crit Thresh"
              << std::right
              << std::setw(crit_alarm_width) << "High Crit Alarm"
              << std::right
              << std::setw(warn_thresh_width) << "Min Warn Thresh"
              << std::right
              << std::setw(warn_alarm_width) << "Low Warn Alarm"
              << std::right
              << std::setw(warn_thresh_width) << "Max Warn Thresh"
              << std::right
              << std::setw(warn_alarm_width) << "High Warn Alarm"
              << std::endl;
}


int CLI::display_all() 
{
    std::cout << "*******************FANS***************************" << std::endl;
    display_fans();
    std::cout << "*******************TEMPERATURES*******************" << std::endl;
    printsplash();
    display_temps();
    std::cout << "*******************VOLTAGES***********************" << std::endl;
    printsplash();
    display_ADC();
    std::cout << "*******************CURRENTS***********************" << std::endl;
    printsplash();
    display_current();
    std::cout << "*******************POWER**************************" << std::endl;
    printsplash();
    display_power();
    return 0;
}

int CLI::display_fans() 
{
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
    std::string pwmstub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tachstub = "/xyz/openbmc_project/sensors/fan_tach";

    std::string property = "Value";
    std::cout << std::string(name_width, '_') << std::endl;
    std::cout << std::left  << std::setw(name_width) << "PWM" << std::endl;
    std::cout << std::string(name_width, '_') << std::endl;
    printsplash();
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    std::string fanName;

    std::vector<std::string> paths;
    
    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, pwmstub, sense_interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No pwm sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << pwmstub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No pwm sensors found under " << pwmstub << std::endl;
        return 0;
    }

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, pwmstub, sense_interface, 0)) {

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            fanName = path.substr(1 + itr);
        }

        double value      = phosphor::interface::util::getProperty<double>(bus, path, sense_interface, property);
        std::cout << std::left  << std::setw(name_width) << fanName
            << std::right << std::setw(value_width - 6) << value << " %PWM"
            << std::right << std::setw(crit_thresh_width) << "N/A"
            << std::right << std::setw(crit_alarm_width) << "N/A"
            << std::right << std::setw(crit_thresh_width) << "N/A"
            << std::right << std::setw(crit_alarm_width) << "N/A"
            << std::right << std::setw(warn_thresh_width) << "N/A"
            << std::right << std::setw(warn_alarm_width) << "N/A"
            << std::right << std::setw(warn_thresh_width) << "N/A"
            << std::right << std::setw(warn_alarm_width) << "N/A"
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    std::cout << std::left  << std::setw(name_width) << "RPM" << std::endl;
    std::cout << std::string(name_width, '_') << std::endl;
    printsplash();
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;

    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, tachstub, sense_interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No tach sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << tachstub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No tach sensors found under " << tachstub << std::endl;
        return 0;
    }

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tachstub, sense_interface, 0)) {
        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            fanName = path.substr(1 + itr);
        }
        double value      = phosphor::interface::util::getProperty<double>(bus, path, sense_interface, property);
        double min_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        double max_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        bool low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmLow");
        bool high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmHigh");
        double warn_min_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningLow");
        double warn_max_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningHigh");
        bool warn_low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmLow");
        bool warn_high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmHigh");

        if (std::abs(value) < 1e-20) value = 0.0;

        std::cout << std::left  << std::setw(name_width) << fanName
            << std::right << std::setw(value_width - 5) << value << " RPM"
            << std::right << std::setw(crit_thresh_width) << min_thresh
            << std::right << std::setw(crit_alarm_width) << (low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(crit_thresh_width) << max_thresh
            << std::right << std::setw(crit_alarm_width) << (high_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_min_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_max_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_high_alarm ? "Alarm Active" : "No Alarm")
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    return 0;
}

int CLI::display_temps() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
    std::string service = "xyz.openbmc_project.temperatureHwmon";
    std::string stub = "/xyz/openbmc_project/sensors/temperature";
    
    std::string property = "Value";
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;

    std::vector<std::string> paths;

    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, stub, interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No temperature sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << stub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No temperature sensors found under " << stub << std::endl;
        return 0;
    }

    std::cout << std::fixed << std::setprecision(4);
    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string tempName;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            tempName = path.substr(1 + itr);
        }

        double value      = phosphor::interface::util::getProperty<double>(bus, path, interface, property);
        double min_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        double max_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        bool low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmLow");
        bool high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmHigh");
        double warn_min_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningLow");
        double warn_max_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningHigh");
        bool warn_low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmLow");
        bool warn_high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmHigh");

        if (std::abs(value) < 1e-20) value = 0.0;

        std::cout << std::left  << std::setw(name_width) << tempName
            << std::right << std::setw(value_width - 3) << value << " C"
            << std::right << std::setw(crit_thresh_width) << min_thresh
            << std::right << std::setw(crit_alarm_width) << (low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(crit_thresh_width) << max_thresh
            << std::right << std::setw(crit_alarm_width) << (high_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_min_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_max_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_high_alarm ? "Alarm Active" : "No Alarm")
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;

    return 0;
}

int CLI::display_ADC() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
    std::string service = "xyz.openbmc_project.ADCHwmon";
    std::string stub = "/xyz/openbmc_project/sensors/voltage";
    
    std::string property = "Value";

    std::vector<std::string> paths;
    
    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, stub, interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No adc sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << stub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No adc sensors found under " << stub << std::endl;
        return 0;
    }

    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;

    std::cout << std::fixed << std::setprecision(4);

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string ADCname;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            ADCname = path.substr(1 + itr);
        }

        double value      = phosphor::interface::util::getProperty<double>(bus, path, interface, property);
        double min_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        double max_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        bool low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmLow");
        bool high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmHigh");
        double warn_min_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningLow");
        double warn_max_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningHigh");
        bool warn_low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmLow");
        bool warn_high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmHigh");

        if (std::abs(value) < 1e-20) value = 0.0;

        std::cout << std::left  << std::setw(name_width) << ADCname
            << std::right << std::setw(value_width - 3) << value << " V"
            << std::right << std::setw(crit_thresh_width) << min_thresh
            << std::right << std::setw(crit_alarm_width) << (low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(crit_thresh_width) << max_thresh
            << std::right << std::setw(crit_alarm_width) << (high_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_min_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_max_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_high_alarm ? "Alarm Active" : "No Alarm")
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    return 0;
}

int CLI::display_current() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
    std::string stub = "/xyz/openbmc_project/sensors/current";
    
    std::string property = "Value";

    std::vector<std::string> paths;
    
    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, stub, interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No current sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << stub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No current sensors found under " << stub << std::endl;
        return 0;
    }

    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    std::cout << std::fixed << std::setprecision(4);

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string currentname;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            currentname = path.substr(1 + itr);
        }
        double value      = phosphor::interface::util::getProperty<double>(bus, path, interface, property);
        double min_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        double max_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        bool low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmLow");
        bool high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmHigh");
        double warn_min_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningLow");
        double warn_max_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningHigh");
        bool warn_low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmLow");
        bool warn_high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmHigh");

        if (std::abs(value) < 1e-20) value = 0.0;

        std::cout << std::left  << std::setw(name_width) << currentname
            << std::right << std::setw(value_width - 3) << value << " A"
            << std::right << std::setw(crit_thresh_width) << min_thresh
            << std::right << std::setw(crit_alarm_width) << (low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(crit_thresh_width) << max_thresh
            << std::right << std::setw(crit_alarm_width) << (high_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_min_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_max_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_high_alarm ? "Alarm Active" : "No Alarm")
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    return 0;
}

int CLI::display_power() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
    std::string stub = "/xyz/openbmc_project/sensors/power";
    
    std::string property = "Value";

    std::vector<std::string> paths;
    
    try
    {
        paths = phosphor::interface::util::getSubTreePathsRaw(
            bus, stub, interface, 0);
    }
    catch (const std::exception& e)
    {
        std::cout << "No power sensors found (or ObjectMapper not ready).\n"
                  << "Path checked: " << stub << std::endl;
        return 0;
    }

    if (paths.empty())
    {
        std::cout << "No power sensors found under " << stub << std::endl;
        return 0;
    }

    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    std::cout << std::fixed << std::setprecision(4);

    for (auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, interface, 0)) {
        std::string powername;

        auto itr = path.rfind("/");
        if (itr != std::string::npos && itr < path.size())
        {
            powername = path.substr(1 + itr);
        }
        double value      = phosphor::interface::util::getProperty<double>(bus, path, interface, property);
        double min_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        double max_thresh = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        bool low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmLow");
        bool high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, "CriticalAlarmHigh");
        double warn_min_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningLow");
        double warn_max_thresh = phosphor::interface::util::getProperty<double>(bus, path, warn_threshold_interface, "WarningHigh");
        bool warn_low_alarm    = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmLow");
        bool warn_high_alarm   = phosphor::interface::util::getProperty<bool>(bus, path, warn_threshold_interface, "WarningAlarmHigh");

        if (std::abs(value) < 1e-20) value = 0.0;

        std::cout << std::left  << std::setw(name_width) << powername
            << std::right << std::setw(value_width - 3) << value << " W"
            << std::right << std::setw(crit_thresh_width) << min_thresh
            << std::right << std::setw(crit_alarm_width) << (low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(crit_thresh_width) << max_thresh
            << std::right << std::setw(crit_alarm_width) << (high_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_min_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_low_alarm ? "Alarm Active" : "No Alarm")
            << std::right << std::setw(warn_thresh_width) << warn_max_thresh
            << std::right << std::setw(warn_alarm_width) << (warn_high_alarm ? "Alarm Active" : "No Alarm")
            << std::endl;
    };
    std::cout << std::string(name_width + value_width + crit_thresh_width*2 + crit_alarm_width*2 + 4 + warn_thresh_width*2 + warn_alarm_width*2, '_') << std::endl;
    return 0;
}

int CLI::display_services()
{

    std::cout << std::left
    << std::setw(40) << "Name"
    << std::right
    << std::setw(20) << "Status"
    << std::endl;

    std::cout << std::string(65, '_') << std::endl;

    std::cout << std::left  << std::setw(40) << "LCR_temp_sensors.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_temp_sensors.service") ? "Active" : "Inactive") << std::endl;

    std::cout << std::left  << std::setw(40) << "LCR_ADC_sensors.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_ADC_sensors.service") ? "Active" : "Inactive") << std::endl;
    
    std::cout << std::left  << std::setw(40) << "LCR_fan_controller.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_fan_controller.service") ? "Active" : "Inactive") << std::endl;
    
    std::cout << std::left  << std::setw(40) << "LCR_Mandatory_Sensors.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_Mandatory_Sensors.service") ? "Active" : "Inactive") << std::endl;
        
    std::cout << std::left  << std::setw(40) << "LCR_GPIO_Mon.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_GPIO_Mon.service") ? "Active" : "Inactive") << std::endl;
    
    std::cout << std::left  << std::setw(40) << "phosphor-ipmi-host.service" << std::right << std::setw(20) 
        << (isServiceActive("phosphor-ipmi-host.service") ? "Active" : "Inactive") << std::endl;

    std::cout << std::left  << std::setw(40) << "LCR_MANAGER.service" << std::right << std::setw(20) 
        << (isServiceActive("LCR_MANAGER.service") ? "Active" : "Inactive") << std::endl;

    std::cout << std::string(65, '_') << std::endl;

    return 0;

}

int CLI::display_alarms() 
{
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string warn_threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Warning";
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
        std::cout << tempName << "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
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
        std::cout << ADCName << "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
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
        std::cout << fanName << "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalLow")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow")) ? "Alarm Active" : "No Alarm") <<
        "\t" << phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, std::string("CriticalHigh")) <<
        "\t" << (phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh")) ? "Alarm Active" : "No Alarm") <<
        std::endl;
    }

    return 0;
}

int CLI::set_threshold(std::string type, std::string itemname, std::string highorlow, double value) 
{
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
