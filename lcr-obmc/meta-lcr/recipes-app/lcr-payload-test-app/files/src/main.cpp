
#include "../include/payloadtest.hpp"

using namespace std;

int main()
{
    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.payloadTest");

    sdbusplus::asio::object_server obj_server(bus);

    PayloadTest testinst(bus, obj_server);

    testinst.runtest();

    io.run();

    return 0;
}
