
#include "../include/fan_manipulation.hpp"

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    fan_loop fanInstance;
    double ts = fanInstance.get_ts();
    int tsint = (int)(ts * 1000);
    while (true) {
        int ret;
        ret = fanInstance.read_temperatures();
        ret = fanInstance.control_loop();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(tsint));
    }

    return 0;
}
