#include "../include/fan_manipulation.hpp"


fan_loop::fan_loop() : bus(sdbusplus::bus::new_default()),
                             object_manager(std::make_unique<ObjectManagerIface>(bus, "/xyz/openbmc_project/sensors"))
                             
{
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop init enter");
    bus.request_name("xyz.openbmc_project.fancontroller");

    controller = std::make_unique<pid_control::ec::pid_info_t>();

    initialize_temps();

    initialize_controller();

    get_fan_pwms();
    
    get_temp_thresholds();
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop init exit");
}    

fan_loop::~fan_loop()
{
    ;
}

void fan_loop::initialize_temps() {
    phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps enter");
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string stub = "/xyz/openbmc_project/sensors/temperature";
    
    for (const auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, stub, sense_interface, 0)) {
        auto instance = std::make_unique<temperature_sensor>();
        instance->path = path;
        
        auto itr = path.rfind("/");
        std::string name;
        if (itr != std::string::npos && itr < path.size())
        {
            name = path.substr(1 + itr);
        };
        instance->name=name;
        temps.push_back(std::move(instance));
    };
    phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps exit");
}

void fan_loop::initialize_controller() {

    nlohmann::json data;
    phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller enter");
    controller->initialized=true;
    phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller going to read json");
    try {
        data = readJson("/usr/share/pid/pid.json");

        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller linear and setpoint");

        linear = data["linear"].get<bool>();
        setpoint = data["setpoint"].get<double>();
        ts = data["ts"].get<double>();

        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller coefficients");
        controller->ts = ts;
        controller->proportionalCoeff = data["proportionalCoeff"].get<double>();
        controller->integralCoeff = data["integralCoeff"].get<double>();
        controller->derivativeCoeff = data["derivativeCoeff"].get<double>();
        controller->feedFwdOffset = data["feedFwdOffset"].get<double>();
        controller->feedFwdGain = data["feedFwdGain"].get<double>();

        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller integral limits");
        controller->integralLimit.min = data["integralLimit"]["min"].get<double>();
        controller->integralLimit.max = data["integralLimit"]["max"].get<double>();

        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller out limits");
        controller->outLim.min = data["outLim"]["min"].get<double>();
        controller->outLim.max = data["outLim"]["max"].get<double>();

        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller slews");
        controller->slewNeg = data["slewNeg"].get<double>();
        controller->slewPos = data["slewPos"].get<double>();
        
        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller hystereses");
        controller->positiveHysteresis = data["positiveHysteresis"].get<double>();
        controller->negativeHysteresis = data["negativeHysteresis"].get<double>();
        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller exit");

    } catch (...) { // Catch-all block for any unhandled exception
        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller json read failed");
        return;
    }
    
}

nlohmann::json fan_loop::readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

int fan_loop::read_temperatures() {
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string property = "Value";
    for (const auto& tempinst : temps) {
        try {
            tempinst->value = phosphor::interface::util::getProperty<double>(bus, tempinst->path, sense_interface, property);
        } catch (...) {
            //set temperature to very low if getProperty fails. This way, this tempinst is ignored in fan controller as it is nowhere near the max temp.
            phosphor::logging::log<phosphor::logging::level::INFO>(("failure at: " + tempinst->path).c_str());
            tempinst->value = -20;
        }
    }

    return 0;
}

int fan_loop::get_fan_pwms() {

    phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms enter");

    std::string pwmstub = "/xyz/openbmc_project/control/fanpwm";
    std::string pwm_interface = "xyz.openbmc_project.Control.FanPwm";

    for (const auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, pwmstub, pwm_interface, 0)) {
        pwm_paths.push_back(path);
        auto inst = std::make_unique<fan>();
        inst->path = path;
        fans.push_back(std::move(inst));
    }

    for (const auto& fan : fans) {
        fan->pwm = 0;
    }

    phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms exit");

    return 0;
}

int fan_loop::get_temp_thresholds() {

    phosphor::logging::log<phosphor::logging::level::INFO>("get_temp_thresholds enter");
    std::string tempstub = "/xyz/openbmc_project/sensors/temperature";
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";

    for (const auto& path : phosphor::interface::util::getSubTreePathsRaw(bus, tempstub, threshold_interface, 0)) {
        temp_min = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalLow");
        temp_max = phosphor::interface::util::getProperty<double>(bus, path, threshold_interface, "CriticalHigh");
        break;
    }

    phosphor::logging::log<phosphor::logging::level::INFO>("get_temp_thresholds exit");

    return 0;
}

int fan_loop::control_loop() {

    bus.process_discard();
    bus.wait(std::chrono::milliseconds(1000));

    nlohmann::json data = readJson("/usr/share/pid/pid.json");
    setpoint = data["setpoint"].get<double>();

    std::string pwm_interface = "xyz.openbmc_project.Control.FanPwm";

    double temp_high = 0;
    double temp_avg = 0;
    double temp_sum = 0;
    double calc_val = 0;
    int nTemps = 0;

    for (const auto& tempinst : temps) {
        temp_sum += tempinst->value;
        temp_high = (tempinst->value > temp_high) ? tempinst->value : temp_high;
        nTemps += 1;
    }
    // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop high temp: " + std::to_string(temp_high)).c_str());
    temp_avg = temp_sum / nTemps;

    double temp_max_norm = temp_max - setpoint;
    // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop linear: " + std::to_string(linear)).c_str());
    if (linear) {
        
        calc_val = pow(((temp_high - setpoint) / temp_max_norm), 1.6) * 255;

        if (temp_high < setpoint) {
            calc_val=0;
        } else if (temp_high > temp_max) {
            calc_val=255;
        }

        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop output of linear: " + std::to_string(calc_val)).c_str());
        for (const auto& fan : fans) {
            fan->pwm = (int)calc_val;
            if (fan->pwm < 75) {
                fan->pwm = 75;
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop linear attempting to control: " + path).c_str());
            phosphor::interface::util::setProperty<uint64_t>(bus, fan->path,
                pwm_interface, "Target",
                fan->pwm);
        }
    } else {
        double output;
        output = pid_control::ec::pid(controller.get(), temp_high, setpoint, nullptr);

        if (temp_high < setpoint) {
            output=0;
        }
        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop output of PID: " + std::to_string(output)).c_str());
        for (const auto& fan : fans) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop linear attempting to control: " + path).c_str());
            fan->pwm = fan->pwm - (int)output;
            
            if (fan->pwm > 255) {
                fan->pwm = 255;
            } else if (fan->pwm < 0) {
                fan->pwm = 0;
            }

            phosphor::interface::util::setProperty<uint64_t>(bus, fan->path,
                pwm_interface, "Target",
                fan->pwm);
        }
        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop new PWMs: " + std::to_string(*curPwms[0])).c_str());
    }
    return 0;
}

double fan_loop::get_ts(){
    return ts;
}