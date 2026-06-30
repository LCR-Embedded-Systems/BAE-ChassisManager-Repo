#include "../include/IPMBLinkSensor.hpp"


/*
####################################################
IPMB LINK SENSOR DEFINITION
####################################################
*/

IPMBLinkSensor::IPMBLinkSensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
                sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context()) {
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor initialization");
    sensor_path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_IPMB_LINK";
    uint8_t ipmb_b_status;
    ipmb_b_status |= (1 << 7);
    ipmb_b_status |= (1 << 3);
    current_value = static_cast<double>(ipmb_b_status);
    byte3 = static_cast<double>(1 << 5);
}

IPMBLinkSensor::~IPMBLinkSensor() {
    timer.cancel();
}

void IPMBLinkSensor::emit_value_changed() {
    phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor emit_value_changed enter");
    auto msg = conn->new_signal(sensor_path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    std::string value_intf = "xyz.openbmc_project.Sensor.Value";
    std::map<std::string, std::variant<double>> changed{{"Value", current_value}};
    std::vector<std::string> invalidated{};
    msg.append(value_intf, changed, invalidated);
    conn->async_send(msg, [](const boost::system::error_code& ec, sdbusplus::message::message& m) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor emit_value_changed async send failure");
        }
    });
    phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor emit_value_changed exit");
}

void IPMBLinkSensor::expose_sensor() {
    phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor expose_sensor enter");
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
    phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor expose_sensor exit");
}

void IPMBLinkSensor::async_update_interface() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor async_update_interface enter");
    async_get_ipmb_status([this](uint8_t val) {
        double new_val = static_cast<double>(val);
        if (new_val != this->byte4) {
            this->byte4 = new_val;
            if (this->value_iface) {
                this->emit_value_changed();
            }
        }
        this->schedule_update();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor async_update_interface exit");
}
// Schedule the next periodic update
void IPMBLinkSensor::schedule_update() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_update_interface();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("IPMBLinkSensor schedule_update exit");
}

// Asynchronous async_get_ipmb_status with short-circuit on first alarm
void IPMBLinkSensor::async_get_ipmb_status(std::function<void(uint8_t)> callback) {
    std::thread([this, callback]() {
        uint8_t result = 0x00;  // default = no flag / normal

        bool ipmb_a;
        bool ipmb_b;
        try {
            std::ifstream file("/usr/share/ipmb_a_enable");
            if (file.is_open()) {
                int flag_value = 0;
                if (file >> flag_value) {
                    if (flag_value == 1) {
                        ipmb_a = true;  
                    } else {
                        ipmb_a = false;
                    }
                }
                file.close();
            }
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to read result_flag file",
                phosphor::logging::entry("ERROR=%s", e.what()));
        }

        try {
            std::ifstream file("/usr/share/ipmb_b_enable");
            if (file.is_open()) {
                int flag_value = 0;
                if (file >> flag_value) {
                    if (flag_value == 1) {
                        ipmb_b = true;  // flag is set, test passed
                    } else {
                        ipmb_b = false;  // flag is 0, test failed
                    }
                }
                file.close();
            }
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to read result_flag file",
                phosphor::logging::entry("ERROR=%s", e.what()));
        }

        if (ipmb_a && ipmb_b) {
            result |= (1 << 3);
        } else if (!ipmb_a && ipmb_b) {
            result |= (1 << 2);
        } else if (ipmb_a && !ipmb_b) {
            result |= (1 << 1);
        } else if (!ipmb_a && !ipmb_b) {
            result |= (1);
        }

        // Call the callback with the result
        callback(result);

    }).detach();  // fire-and-forget
}

// Start the asynchronous update cycle
void IPMBLinkSensor::start_updates() {
    schedule_update();
}