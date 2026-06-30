
#include "../include/lcr_gpio_mon.hpp"

int main()
{
    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.GPIOMon");

    sdbusplus::asio::object_server obj_server(bus);

    GpioMonitor monitor(bus, obj_server);

    monitor.schedule_update();

    io.run();
    
    return 0;
}
