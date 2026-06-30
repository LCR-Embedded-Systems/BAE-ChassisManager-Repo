
#include "../include/tmp_manipulation.hpp"

int main()
{

    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.temperatureHwmon");

    sdbusplus::asio::object_server obj_server(bus);

    Temp_Sensor instanceTemp(bus, obj_server);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    instanceTemp.schedule_update();

    io.run();

    return 0;
}
