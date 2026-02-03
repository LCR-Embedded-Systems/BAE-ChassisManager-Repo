
#include "../include/MANAGER.hpp"

//testing build addition

using namespace std;

int main()
{
    Manager chm;

    thread logThread([&chm]() {
        int heartbeat = 1;
        while(true) {
            chm.write_logs_to_flash();
            std::this_thread::sleep_for(std::chrono::milliseconds(12000));
            if (heartbeat % 20 == 0) {
                heartbeat = 1;
                chm.write_heartbeat();
            }
            heartbeat++;
        }
    });

    while (chm.wait_for_switch() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}

    while (chm.get_fan_controller_status() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    
    while (chm.set_ps() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}

    while (chm.update_readings() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}

    while (chm.set_sr() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}

    chm.write_logs_to_flash();

    thread watcherThread([&chm]() {
        chm.watch_alarms();
    });

    //make thread for checking if gpio service is active and publishing. Publish value of sysreset and ps enable when it is active.
    thread gpioThread([&chm]() {
        chm.watch_gpios();
    });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // had an alarm go high
    }

    return 0;
}
