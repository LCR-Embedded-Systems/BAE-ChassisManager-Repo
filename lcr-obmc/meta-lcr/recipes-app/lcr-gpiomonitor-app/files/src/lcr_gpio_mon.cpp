#include "../include/lcr_gpio_mon.hpp"

using json = nlohmann::json;

json readJson(std::string path) {
    std::ifstream f(path);

    json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

bool gpio_chip_exists(const std::string& chip_path) {
    try {
        gpiod::chip chip(chip_path);
        return true;
    } catch (const std::system_error& e) {
        if (e.code().value() == ENOENT ||    // No such file
            e.code().value() == ENODEV ||     // No such device
            e.code().value() == EACCES) {     // Permission denied
            return false;
        }
        throw;
    }
}

GpioMonitor::GpioMonitor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
        sdbusplus::asio::object_server& server_ref)
        : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context()){

    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("GpioMonitor initialization");
    
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    ts = 0.1;
    build_gpio_list();
    initallpins();
    
    expose_gpios();

};

GpioMonitor::~GpioMonitor() {;};

gpio_info GpioMonitor::populate_info(bool input, int chip, int line, int status, std::string name) {
    gpio_info populated_gpio;
    populated_gpio.input = input;
    populated_gpio.chip = chip;
    populated_gpio.line = line;
    populated_gpio.status = status;
    populated_gpio.name = name;

    return populated_gpio;
}

void GpioMonitor::build_gpio_list() {

    phosphor::logging::log<phosphor::logging::level::INFO>("populating gpio list");

    BP_GPIO_OUT0 = populate_info(false, 0, 8, 0, std::string("BP_GPIO_OUT0"));
    BP_GPIO_OUT1 = populate_info(false, 0, 9, 0, std::string("BP_GPIO_OUT1"));
    BP_GPIO_OUT2 = populate_info(false, 0, 10, 0, std::string("BP_GPIO_OUT2"));
    BP_GPIO_OUT3 = populate_info(false, 0, 11, 0, std::string("BP_GPIO_OUT3"));
    BP_GPIO_OUT4 = populate_info(false, 0, 12, 0, std::string("BP_GPIO_OUT4"));
    BP_GPIO_OUT5 = populate_info(false, 0, 13, 0, std::string("BP_GPIO_OUT5"));
    BP_GPIO_OUT6 = populate_info(false, 0, 14, 0, std::string("BP_GPIO_OUT6"));
    BP_GPIO_OUT7 = populate_info(false, 0, 15, 0, std::string("BP_GPIO_OUT7"));
    
    BP_GPIO_IN0 = populate_info(true, 0, 0, 0, std::string("BP_GPIO_IN0"));
    BP_GPIO_IN1 = populate_info(true, 0, 1, 0, std::string("BP_GPIO_IN1"));
    BP_GPIO_IN2 = populate_info(true, 0, 2, 0, std::string("BP_GPIO_IN2"));
    BP_GPIO_IN3 = populate_info(true, 0, 3, 0, std::string("BP_GPIO_IN3"));
    BP_GPIO_IN4 = populate_info(true, 0, 4, 0, std::string("BP_GPIO_IN4"));
    BP_GPIO_IN5 = populate_info(true, 0, 5, 0, std::string("BP_GPIO_IN5"));
    BP_GPIO_IN6 = populate_info(true, 0, 6, 0, std::string("BP_GPIO_IN6"));
    BP_GPIO_IN7 = populate_info(true, 0, 7, 0, std::string("BP_GPIO_IN7"));

    sysreset = populate_info(false, 1, 0, 0, std::string("sysreset"));
    nvmro = populate_info(true, 2, 0, 0, std::string("nvmro"));
    gdiscrete = populate_info(false, 3, 0, 0, std::string("gdiscrete"));
    psenable = populate_info(false, 3, 3, 0, std::string("psenable"));
    ps1inh = populate_info(false, 3, 2, 0, std::string("ps1inh"));
    ps1fail = populate_info(true, 4, 0, 0, std::string("ps1fail"));
    ps2inh = populate_info(false, 3, 1, 0, std::string("ps2inh"));
    ps2fail = populate_info(true, 4, 1, 0, std::string("ps2fail"));

    available_gpios[BP_GPIO_OUT0.name] = BP_GPIO_OUT0;
    available_gpios[BP_GPIO_OUT1.name] = BP_GPIO_OUT1;
    available_gpios[BP_GPIO_OUT2.name] = BP_GPIO_OUT2;
    available_gpios[BP_GPIO_OUT3.name] = BP_GPIO_OUT3;
    available_gpios[BP_GPIO_OUT4.name] = BP_GPIO_OUT4;
    available_gpios[BP_GPIO_OUT5.name] = BP_GPIO_OUT5;
    available_gpios[BP_GPIO_OUT6.name] = BP_GPIO_OUT6;
    available_gpios[BP_GPIO_OUT7.name] = BP_GPIO_OUT7;

    available_gpios[BP_GPIO_IN0.name] = BP_GPIO_IN0;
    available_gpios[BP_GPIO_IN1.name] = BP_GPIO_IN1;
    available_gpios[BP_GPIO_IN2.name] = BP_GPIO_IN2;
    available_gpios[BP_GPIO_IN3.name] = BP_GPIO_IN3;
    available_gpios[BP_GPIO_IN4.name] = BP_GPIO_IN4;
    available_gpios[BP_GPIO_IN5.name] = BP_GPIO_IN5;
    available_gpios[BP_GPIO_IN6.name] = BP_GPIO_IN6;
    available_gpios[BP_GPIO_IN7.name] = BP_GPIO_IN7;
    available_gpios[nvmro.name] = nvmro;
    available_gpios[sysreset.name] = sysreset;
    available_gpios[ps1fail.name] = ps1fail;
    available_gpios[ps2fail.name] = ps2fail;
    available_gpios[gdiscrete.name] = gdiscrete;
    available_gpios[psenable.name] = psenable;
    available_gpios[ps1inh.name] = ps1inh;
    available_gpios[ps2inh.name] = ps2inh;

    chassis_signals[nvmro.name] = nvmro;
    chassis_signals[sysreset.name] = sysreset;
    chassis_signals[ps1fail.name] = ps1fail;
    chassis_signals[ps2fail.name] = ps2fail;
    chassis_signals[gdiscrete.name] = gdiscrete;
    chassis_signals[psenable.name] = psenable;
    chassis_signals[ps1inh.name] = ps1inh;
    chassis_signals[ps2inh.name] = ps2inh;

    for (auto& gpio_chip : configJson["gpioexpanders"].items()) {
        if (gpio_chip_exists("/dev/" + gpio_chip.key())) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_chip.key()).c_str());
            char c_chip = gpio_chip.key().back();
            std::string s_chip(1, c_chip);
            int chip = stoi(s_chip);
            for (auto& gpio : gpio_chip.value().items()) {
                if (gpio.key() != "bus" && gpio.key() != "address") {
                    int line = configJson["gpioexpanders"][gpio_chip.key()][gpio.key()].get<int>();
                    available_gpios[gpio.key()] = populate_info(false, chip, line, 0, gpio.key());
                }
            }
        } else {
            phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_chip.key() + " is not present.").c_str());
        }
    }
}

bool GpioMonitor::readgpio(gpio_info* target_gpio)
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio enter");
    bool ischassissig = false;
    for (auto& pair : chassis_signals) {
        if (pair.second.name == target_gpio->name) {
            ischassissig = true;
            break;
        }
    }
    if (target_gpio->input || !ischassissig) {
        try {

            if (target_gpio == nullptr) {
                phosphor::logging::log<phosphor::logging::level::INFO>("readgpio target gpio does not exist");
                return false;
            }
    
            bool ret_check = false;
    
            // Open the GPIO chip
            gpiod::chip chip("gpiochip" + std::to_string(target_gpio->chip));
            
            // Get the line
            gpiod::line line = chip.get_line(target_gpio->line);
            
            // Request line as output
            line.request({"readgpio", gpiod::line_request::DIRECTION_INPUT, 0});
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio attempting to get value");
            // Read the current value
            int value = line.get_value();
            
            if (value != target_gpio->status) {
                ret_check = true;
            }
            // Update status
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio updating the status");
            target_gpio->status = value;
            line.release();
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio exit");
            return ret_check;
            
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::INFO>("readgpio error, exit");
            return false;
        }
    } else {
        return false;
    }
    
    // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio exit");
}

void GpioMonitor::initallpins() {
    phosphor::logging::log<phosphor::logging::level::INFO>("initallpins enter");

    for (auto& [name, gpio] : available_gpios) {
        bool gpiostatus = readgpio(&gpio);
    }
}

void GpioMonitor::expose_gpios() {
    
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpios enter");

    for (const auto& [name, gpio] : available_gpios) {
    
        expose_gpio(gpio);
    
    }
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpios exit");
}

void GpioMonitor::expose_gpio(const gpio_info& target_gpio) {

    std::string path = "/xyz/openbmc_project/gpio/" + target_gpio.name;
    std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Value");
    std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Association.Definitions");

    phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path).c_str());

    value_iface->register_property_rw<int>(
        "Value",
        sdbusplus::vtable::property_::emits_change,
        [this, name = target_gpio.name](const int& requested_value, int& current_value) -> bool {
            // Setter: Validate/update stored status
            auto it = available_gpios.find(name);
            if (it == available_gpios.end()) {
                // Invalid GPIO - reject the set
                return false;
            }
    
            if (requested_value == current_value) {
                // No change - don't emit signal
                return false;
            }
    
            // Update internal D-Bus value and map
            current_value = requested_value;
            it->second.status = requested_value;
    
            phosphor::logging::log<phosphor::logging::level::INFO>(
                ("D-Bus set Value for " + name + " to " + std::to_string(requested_value)).c_str()
            );
            phosphor::logging::log<phosphor::logging::level::INFO>(
                ("Internal value of " + name + " set to " + std::to_string(it->second.status)).c_str()
            );

            // Success with change - emit signal
            return true;
        },
        [this, name = target_gpio.name](const int&) -> int {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    ("Internal value of " + name + " is currently " + std::to_string(it->second.status)).c_str()
                );
                return it->second.status;
            }
            return -1;  // Fallback
        }
    );

    // Register other required properties for Sensor.Value with defaults
    value_iface->register_property_r<int>(
        "MaxValue",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return 1; }
    );
    value_iface->register_property_r<int>(
        "MinValue",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return 0; }
    );
    value_iface->register_property_r<std::string>(
        "Unit",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.None"); }
    );
    value_iface->register_property_r<int64_t>(
        "Scale",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return int64_t(0); }
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
    assoc_iface->initialize();

    m_value_iface[target_gpio.name] = std::move(value_iface);
    m_assoc_iface[target_gpio.name] = std::move(assoc_iface);
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpio exit");
}

void GpioMonitor::async_readgpio(const std::string& gpio_name, std::function<void(bool, int)> callback) {  // Pass name, callback success and new_status
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readgpio enter");
    boost::asio::post(timer.get_executor(), [this, gpio_name, callback]() {
        int new_status = -1;  // Error value
        try {
            auto it = available_gpios.find(gpio_name);
            if (it == available_gpios.end()) {
                throw std::runtime_error("GPIO not found");
            }
            gpio_info* gpio_ptr = &it->second;  // Pointer to modifiable gpio_info in map
            bool changed = readgpio(gpio_ptr);  // Call sync, which updates status and returns if changed
            new_status = gpio_ptr->status;  // Get updated status
            callback(changed, new_status);
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read GPIO " + gpio_name).c_str());
            callback(false, new_status);
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readgpio exit");
}

void GpioMonitor::async_readallpins() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins enter");

    if (available_gpios.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map (use vector of keys for ordering if needed; here assuming order doesn't matter)
    std::vector<std::string> keys;
    for (const auto& kv : available_gpios) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            return;
        }
        const std::string& gpio_name = keys[*idx];
        ++(*idx);

        async_readgpio(gpio_name, [this, check_next_ptr, gpio_name](bool change, int new_status) {
            if (change) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins change seen, attempting to update bus");
                async_update_bus(gpio_name, new_status);  // Emit always, or re-check
            }
            (*check_next_ptr)();  // Proceed to next GPIO
        });
    };
    (*check_next_ptr)();  // Start chain

    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins exit");
}

void GpioMonitor::async_update_bus(const std::string& gpio_name, bool new_status) {
    
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus enter");
    auto it = m_value_iface.find(gpio_name);
    if (it == m_value_iface.end() || !it->second) {
        return;  // No interface for this GPIO
    }

    // Emit change signal (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, iface = it->second.get(), gpio_name, new_status]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("GPIO status changed", phosphor::logging::entry("NAME=%s", gpio_name.c_str()), phosphor::logging::entry("NEW_STATUS=%d", new_status));
        if (iface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus attempting to update bus for " + gpio_name).c_str());
            iface->signal_property("Value");
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus exit");
}

// Schedule the next periodic update
void GpioMonitor::schedule_update() {
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("GpioMonitor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_readallpins();
    });
}

double GpioMonitor::get_ts() {
    phosphor::logging::log<phosphor::logging::level::INFO>("getting ts");
    return ts;
}
