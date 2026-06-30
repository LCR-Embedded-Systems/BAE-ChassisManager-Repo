#include "../include/PayloadTestStatus.hpp"


/*
####################################################
PAYLOAD TEST STATUS DEFINITION
####################################################
*/

PayloadTestStatus::PayloadTestStatus(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
                sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context()) {
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus initialization");
    sensor_path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_PAYL_STATUS";
}

PayloadTestStatus::~PayloadTestStatus() {
    timer.cancel();
}

void PayloadTestStatus::emit_value_changed() {
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus emit_value_changed enter");
    auto msg = conn->new_signal(sensor_path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    std::string value_intf = "xyz.openbmc_project.Sensor.Value";
    std::map<std::string, std::variant<double>> changed{{"Value", current_value}};
    std::vector<std::string> invalidated{};
    msg.append(value_intf, changed, invalidated);
    conn->async_send(msg, [](const boost::system::error_code& ec, sdbusplus::message::message& m) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus emit_value_changed async send failure");
        }
    });
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus emit_value_changed exit");
}

void PayloadTestStatus::expose_sensor() {
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus expose_sensor enter");
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
    phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus expose_sensor exit");
}

void PayloadTestStatus::async_update_interface() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus async_update_interface enter");
    async_get_payload_status([this](uint8_t val) {
        double new_val = static_cast<double>(val);
        if (new_val != this->byte4) {
            this->byte4 = new_val;
            if (this->value_iface) {
                this->emit_value_changed();
            }
        }
        this->schedule_update();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus async_update_interface exit");
}
// Schedule the next periodic update
void PayloadTestStatus::schedule_update() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_update_interface();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("PayloadTestStatus schedule_update exit");
}

// Asynchronous get_all_alarms_status with short-circuit on first alarm
void PayloadTestStatus::async_get_payload_status(std::function<void(uint8_t)> callback) {
    boost::asio::post(conn->get_io_context(), [this, callback]() {
        uint8_t result = 0x02;  // default = no flag / normal

        try {
            std::ifstream file("/usr/payload_test/status_flag");
            if (file.is_open()) {
                int flag_value = 0;
                if (file >> flag_value) {
                    if (flag_value == 1) {
                        result = 0x02;  // flag is set → test in progress
                    } else {
                        result = 0x01;  // flag is 0 → test over
                    }
                }
                file.close();
            }
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to read status_flag file",
                phosphor::logging::entry("ERROR=%s", e.what()));
        }

        callback(result);
    });
}

// Start the asynchronous update cycle
void PayloadTestStatus::start_updates() {
    schedule_update();
}