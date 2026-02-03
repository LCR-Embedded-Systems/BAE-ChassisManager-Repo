
#include "../include/lcr_gpio_mon.hpp"

int main()
{
    GpioMonitor monitor;
    double ts = monitor.get_ts();
    int tsint = (int)(ts * 1000);
    while (true) {
        monitor.run_monitor();
        std::this_thread::sleep_for(std::chrono::milliseconds(tsint));
    }
    
    return 0;
}
