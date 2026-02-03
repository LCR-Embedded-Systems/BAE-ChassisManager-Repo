#include "../include/lcr_gpio_mon.hpp"

using json = nlohmann::json;

GpioMonitor::GpioMonitor() : bus(sdbusplus::bus::new_default()) {

    bus.request_name("xyz.openbmc_project.GPIOMon");

    phosphor::logging::log<phosphor::logging::level::INFO>("GpioMonitor initialization");
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
    nvmro = populate_info(true, 2, 4, 0, std::string("nvmro"));
    gdiscrete = populate_info(false, 2, 0, 0, std::string("gdiscrete"));
    psenable = populate_info(false, 2, 3, 0, std::string("psenable"));
    ps1inh = populate_info(false, 2, 2, 0, std::string("ps1inh"));
    ps1fail = populate_info(true, 3, 0, 0, std::string("ps1fail"));
    ps2inh = populate_info(false, 2, 1, 0, std::string("ps2inh"));
    ps2fail = populate_info(true, 3, 1, 0, std::string("ps2fail"));

    available_gpios.push_back(BP_GPIO_OUT0);
    available_gpios.push_back(BP_GPIO_OUT1);
    available_gpios.push_back(BP_GPIO_OUT2);
    available_gpios.push_back(BP_GPIO_OUT3);
    available_gpios.push_back(BP_GPIO_OUT4);
    available_gpios.push_back(BP_GPIO_OUT5);
    available_gpios.push_back(BP_GPIO_OUT6);
    available_gpios.push_back(BP_GPIO_OUT7);

    available_gpios.push_back(BP_GPIO_IN0);
    available_gpios.push_back(BP_GPIO_IN1);
    available_gpios.push_back(BP_GPIO_IN2);
    available_gpios.push_back(BP_GPIO_IN3);
    available_gpios.push_back(BP_GPIO_IN4);
    available_gpios.push_back(BP_GPIO_IN5);
    available_gpios.push_back(BP_GPIO_IN6);
    available_gpios.push_back(BP_GPIO_IN7);
    available_gpios.push_back(nvmro);
    available_gpios.push_back(ps1fail);
    available_gpios.push_back(ps2fail);

}

bool GpioMonitor::readgpio(gpio_info* target_gpio)
{
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
        
        // Read the current value
        int value = line.get_value();
        
        if (value != target_gpio->status) {
            ret_check = true;
        }
        // Update status
        target_gpio->status = value;
        line.release();
        return ret_check;
        
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("readgpio error");
        return false;
    }
}

void GpioMonitor::update_bus(gpio_info* target_gpio) {
    gpioIfaces[target_gpio->name]->value(target_gpio->status);
}

void GpioMonitor::readallpins() {

    for (auto& gpio : available_gpios) {
        bool gpiostatus = readgpio(&gpio);
        if (gpiostatus) {
            update_bus(&gpio);
        }
    }
}

void GpioMonitor::initallpins() {
    phosphor::logging::log<phosphor::logging::level::INFO>("initallpins enter");

    for (auto& gpio : available_gpios) {
        bool gpiostatus = readgpio(&gpio);
    }
}

void GpioMonitor::expose_gpio(const gpio_info& target_gpio) {
    std::string path = "/xyz/openbmc_project/gpio/" + target_gpio.name;
    phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path).c_str());
    auto pinIface = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface>>(bus, path.c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>("setting value");
    pinIface->value(target_gpio.status);

    pinIface->minValue(-1);
    pinIface->maxValue(2);

    pinIface->associations({
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
    });

    gpioIfaces[target_gpio.name] = std::move(pinIface);
}

void GpioMonitor::expose_gpios() {
    
    phosphor::logging::log<phosphor::logging::level::INFO>("exposing gpios");

    for (const auto& gpio : available_gpios) {
    
        expose_gpio(gpio);
    
    }

    expose_gpio(sysreset);
    expose_gpio(gdiscrete);
    expose_gpio(psenable);
    expose_gpio(ps1inh);
    expose_gpio(ps2inh);

}

double GpioMonitor::get_ts() {
    phosphor::logging::log<phosphor::logging::level::INFO>("getting ts");
    return ts;
}

void GpioMonitor::run_monitor() {
    bus.process_discard();
    bus.wait(std::chrono::milliseconds(100));
    readallpins();
}
