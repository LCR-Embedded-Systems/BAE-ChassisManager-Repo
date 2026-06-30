
#include "../include/global.hpp"

class PayloadTest
{

    public:

        PayloadTest(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~PayloadTest();

        void runtest();

        std::ofstream result_flag;
        std::ofstream status_flag;

    private:
    
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        boost::asio::steady_timer timer;

        std::shared_ptr<sdbusplus::asio::dbus_interface> iface_;

        bool checktempdevices();
        bool checkvoltagedevices();
        bool checkgpiodevices();
        bool checkfandevices();
        bool checkipmb();

        bool checki2cdevices();
        bool checkservices();
        bool checkethernetphys();
        bool checkRAM();

        void writeresults(bool result);
        void writestatus(int status);

        std::map<std::string, std::string> temp_nametoaddress;
        std::map<std::string, std::string> adc_nametoaddress;
        std::map<std::string, std::string> fan_nametoaddress;
        std::map<std::string, std::string> gpio_nametoaddress;

        nlohmann::json configJson;

        int m_status;

};
