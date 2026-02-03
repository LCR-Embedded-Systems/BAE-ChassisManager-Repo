
#include "../include/tmp_manipulation.hpp"

int main()
{
    //std::string directory = "hwmon2";
    std::vector<Temp_Sensor> sensors_C;
    Temp_Sensor instanceTemp;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    while (true) {
        instanceTemp.update_reading();
        instanceTemp.send_all_readings();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
