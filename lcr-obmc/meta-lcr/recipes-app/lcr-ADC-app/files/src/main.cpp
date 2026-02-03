
#include "../include/ADC_manipulation.hpp"

int main()
{

    ADC_sensor instanceADC;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    while (true) {
        instanceADC.update_readings();
        instanceADC.send_all_readings();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
