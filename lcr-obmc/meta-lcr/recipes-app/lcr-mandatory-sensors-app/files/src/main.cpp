
#include "../include/FRUHealthState.hpp"
#include "../include/FRUStateSensor.hpp"
#include "../include/FRUTemperatureSensor.hpp"
#include "../include/FRUVoltageSensor.hpp"
#include "../include/IPMBLinkSensor.hpp"
#include "../include/PayloadModeSensor.hpp"
#include "../include/PayloadTestResults.hpp"
#include "../include/PayloadTestStatus.hpp"

int main() {
    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.MandatorySensors");

    sdbusplus::asio::object_server obj_server(bus);

    FRUHealthState       fru_health_state(bus, obj_server);
    FRUStateSensor       fru_state_state(bus, obj_server);
    FRUTemperatureSensor fru_temperature_sensor(bus, obj_server);
    FRUVoltageSensor     fru_voltage_sensor(bus, obj_server);
    IPMBLinkSensor       ipmb_link_sensor(bus, obj_server);
    PayloadModeSensor    payload_mode_sensor(bus, obj_server);
    PayloadTestResults   payload_test_results(bus, obj_server);
    PayloadTestStatus    payload_test_status(bus, obj_server);

    fru_health_state.expose_sensor();
    fru_state_state.expose_sensor();
    fru_temperature_sensor.expose_sensor();
    fru_voltage_sensor.expose_sensor();
    ipmb_link_sensor.expose_sensor();
    payload_mode_sensor.expose_sensor();
    payload_test_results.expose_sensor();
    payload_test_status.expose_sensor();

    fru_health_state.start_updates();
    fru_state_state.start_updates();
    fru_temperature_sensor.start_updates();
    fru_voltage_sensor.start_updates();
    ipmb_link_sensor.start_updates();
    payload_mode_sensor.start_updates();
    payload_test_results.start_updates();
    payload_test_status.start_updates();

    io.run();  // Runs the event loop asynchronously

    return 0;
}