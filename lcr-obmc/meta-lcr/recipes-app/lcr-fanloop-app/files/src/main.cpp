
#include "../include/fan_manipulation.hpp"

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    fan_loop fanInstance;
    double ts = fanInstance.get_ts();
    int tsint = (int)(ts * 1000);
    int recheck_flag = 0;
    while (true) {
        int ret;
        ret = fanInstance.read_temperatures();
        ret = fanInstance.control_loop();
        if (recheck_flag % 10 == 0) {
            fanInstance.get_fan_pwms();
            recheck_flag = 0;    
        }
        recheck_flag++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(tsint));
    }

    return 0;
}
