#include "../include/FRUTemperatureSensor.hpp"


/*
####################################################
FRU TEMPERATURE STATE DEFINITION
####################################################
*/

FRUTemperatureSensor::FRUTemperatureSensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
                sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context()) {
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor initialization");
    sensor_path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_TEMPERATURE";
    byte3 = static_cast<double>(1 << 5);
}

FRUTemperatureSensor::~FRUTemperatureSensor() {
    timer.cancel();
}

void FRUTemperatureSensor::emit_value_changed() {
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor emit_value_changed enter");
    auto msg = conn->new_signal(sensor_path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    std::string value_intf = "xyz.openbmc_project.Sensor.Value";
    std::map<std::string, std::variant<double>> changed{{"Value", current_value}};
    std::vector<std::string> invalidated{};
    msg.append(value_intf, changed, invalidated);
    conn->async_send(msg, [](const boost::system::error_code& ec, sdbusplus::message::message& m) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor emit_value_changed async send failure");
        }
    });
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor emit_value_changed exit");
}

void FRUTemperatureSensor::expose_sensor() {
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor expose_sensor enter");
    value_iface = obj_server.add_unique_interface(sensor_path, "xyz.openbmc_project.Sensor.Value");

    // Register Value as read-only with emits_change
    value_iface->register_property_r<double>(
        "Value",
        sdbusplus::vtable::property_::emits_change,
        [this](const auto&) { return current_value; }
    );
    value_iface->register_property_r<double>(
        "Byte3",
        sdbusplus::vtable::property_::emits_change,
        [this](const auto&) { return byte3; }
    );

    value_iface->register_property_r<double>(
        "Byte4",
        sdbusplus::vtable::property_::emits_change,
        [this](const auto&) { return byte4; }
    );
    // Register other required properties for Sensor.Value with defaults
    value_iface->register_property_r<int64_t>(
        "MaxValue",
        sdbusplus::vtable::property_::const_,
        [](const auto&) { return 1; }
    );
    value_iface->register_property_r<int64_t>(
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

    value_iface->initialize();

    assoc_iface = obj_server.add_unique_interface(sensor_path, "xyz.openbmc_project.Association.Definitions");
    
    // Register Associations as read-only
    assoc_iface->register_property_r<std::vector<std::tuple<std::string, std::string, std::string>>>(
        "Associations",
        sdbusplus::vtable::property_::const_,
        [](const auto&) {
            return std::vector<std::tuple<std::string, std::string, std::string>>{
                {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
            };
        }
    );

    assoc_iface->initialize();
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor expose_sensor exit");
}

void FRUTemperatureSensor::async_update_interface() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_update_interface enter");
    async_get_all_alarms_status([this](uint8_t val) {
        double new_val = static_cast<double>(val);
        if (new_val != this->byte4) {
            this->byte4 = new_val;
            if (this->value_iface) {
                this->emit_value_changed();
            }
        }
        this->schedule_update();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_update_interface exit");
}
// Schedule the next periodic update
void FRUTemperatureSensor::schedule_update() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_update_interface();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor schedule_update exit");
}

// Asynchronous get_all_alarms_status with short-circuit on first alarm
void FRUTemperatureSensor::async_get_all_alarms_status(std::function<void(uint8_t)> callback) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status enter");
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string stub = "/xyz/openbmc_project/sensors/temperature";

    auto alarms = std::make_shared<uint8_t>(0x00);

    auto remaining = std::make_shared<std::atomic<int>>(4);

    auto done = [callback, alarms, remaining]() {
        if (--(*remaining) == 0) {
            callback(*alarms);
        }
    };

    phosphor::interface::util::async_upper_crit_alarm_check(conn, stub, threshold_interface, 0,
        [alarms, done](bool res) {
            if (res) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status an alarm is set");
                *alarms |= (1 << 4);
            }
            done();
        });

    phosphor::interface::util::async_upper_noncrit_alarm_check(conn, stub, threshold_interface, 0,
        [alarms, done](bool res) {
            if (res) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status an alarm is set");
                *alarms |= (1 << 3);
            }
            done();
        });

    phosphor::interface::util::async_lower_crit_alarm_check(conn, stub, threshold_interface, 0,
        [alarms, done](bool res) {
            if (res) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status an alarm is set");
                *alarms |= (1 << 1);
            }
            done();
        });

    phosphor::interface::util::async_lower_noncrit_alarm_check(conn, stub, threshold_interface, 0,
        [alarms, done](bool res) {
            if (res) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status an alarm is set");
                *alarms |= (1);
            }
            done();
        });

    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUTemperatureSensor async_get_all_alarms_status exit");
}

// Start the asynchronous update cycle
void FRUTemperatureSensor::start_updates() {
    schedule_update();
}