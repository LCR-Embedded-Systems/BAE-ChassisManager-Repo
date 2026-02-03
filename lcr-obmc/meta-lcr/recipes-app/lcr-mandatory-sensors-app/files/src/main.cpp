
#include "../include/mandatorySensors.hpp"

sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    bus.request_name("xyz.openbmc_project.MandatorySensors");

    FRUHealthState fru_health_state(bus);

    fru_health_state.expose_sensor();
    fru_health_state.update_interface();

    while (true) {
        bus.process_discard();
        bus.wait(std::chrono::milliseconds(1000));
        fru_health_state.update_interface();
    }

    return 0;
}
