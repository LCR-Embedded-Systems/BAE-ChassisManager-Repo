
#include "../include/fan_monitor.hpp"

int main()
{

    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.lcr_fan_monitor");

    sdbusplus::asio::object_server obj_server(bus);

    fan_monitor monitor(bus, obj_server);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    monitor.schedule_update();

    io.run();

    return 0;
}
