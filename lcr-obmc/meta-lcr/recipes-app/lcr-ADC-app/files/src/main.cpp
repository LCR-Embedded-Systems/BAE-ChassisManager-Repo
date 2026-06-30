
#include "../include/ADC_manipulation.hpp"

int main()
{

    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.ADCHwmon");

    sdbusplus::asio::object_server obj_server(bus);

    ADC_sensor instanceADC(bus, obj_server);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    instanceADC.schedule_update();

    io.run();

    return 0;
}
