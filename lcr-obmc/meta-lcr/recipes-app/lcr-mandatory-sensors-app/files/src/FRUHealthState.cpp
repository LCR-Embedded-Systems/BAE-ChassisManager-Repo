#include "../include/FRUHealthState.hpp"


/*
####################################################
FRU HEALTH STATE DEFINITION
####################################################
*/

FRUHealthState::FRUHealthState(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
                sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context()) {
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState initialization");
    sensor_path = "/xyz/openbmc_project/sensors/mandatoryNumbers/VSO_FRU_HEALTH";
    byte3 = static_cast<double>(1 << 5);
}

FRUHealthState::~FRUHealthState() {
    timer.cancel();
}

void FRUHealthState::emit_value_changed() {
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState emit_value_changed enter");
    auto msg = conn->new_signal(sensor_path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    std::string value_intf = "xyz.openbmc_project.Sensor.Value";
    std::map<std::string, std::variant<double>> changed{{"Value", current_value}};
    std::vector<std::string> invalidated{};
    msg.append(value_intf, changed, invalidated);
    conn->async_send(msg, [](const boost::system::error_code& ec, sdbusplus::message::message& m) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState emit_value_changed async send failure");
        }
    });
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState emit_value_changed exit");
}

void FRUHealthState::expose_sensor() {
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState expose_sensor enter");
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
        [](const auto&) { return 64; }
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
    phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState expose_sensor exit");
}

void FRUHealthState::async_update_interface() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_update_interface enter");
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
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_update_interface exit");
}
// Schedule the next periodic update
void FRUHealthState::schedule_update() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_update_interface();
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState schedule_update exit");
}

// Asynchronous get_all_alarms_status with short-circuit on first alarm
void FRUHealthState::async_get_all_alarms_status(std::function<void(uint8_t)> callback) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_get_all_alarms_status enter");
    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string tempstub = "/xyz/openbmc_project/sensors/temperature";
    std::string tachstub = "/xyz/openbmc_project/sensors/fan_tach";
    std::string adcstub = "/xyz/openbmc_project/sensors/voltage";

    std::vector<std::string> stubs = {tempstub, tachstub, adcstub}; stubs = {tempstub, tachstub, adcstub};
    auto check_next = std::make_shared<size_t>(0);
    auto alarm_active = std::make_shared<bool>(false);

    auto done = [callback, alarm_active]() {
        callback(*alarm_active ? 0x02 : 0x01);
    };

    auto check_ptr = std::make_shared<std::function<void()> >();
    *check_ptr = [this, stubs, check_next, alarm_active, done, threshold_interface, check_ptr]() mutable {
        if (*alarm_active || *check_next >= stubs.size()) {
            // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_get_all_alarms_status finished checking alarms. running done");
            done();
            return;
        }
        std::string stub = stubs[*check_next];
        ++(*check_next);
        phosphor::interface::util::async_alarm_check(conn, stub, threshold_interface, 0,
            [alarm_active, check_ptr](bool res) {
                if (res) {
                    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_get_all_alarms_status an alarm is set");
                    *alarm_active = true;
                }
                (*check_ptr)();
            });
    };
    (*check_ptr)();
    // phosphor::logging::log<phosphor::logging::level::INFO>("FRUHealthState async_get_all_alarms_status exit");
}

// Start the asynchronous update cycle
void FRUHealthState::start_updates() {
    schedule_update();
}