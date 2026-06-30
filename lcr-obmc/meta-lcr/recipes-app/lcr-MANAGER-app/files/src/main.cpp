
#include "../include/MANAGER.hpp"

//testing build addition

using namespace std;

/*

GPI0 is used for the front panel switch
Drive ps inhibit 1 based on that. Always follow that signal
Reflect whatever is done on ps inhibit 2


*/

int main()
{
    Manager chm;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    thread logThread([&chm]() {
        int heartbeat = 18;
        while(true) {
            chm.write_logs_to_flash();
            std::this_thread::sleep_for(std::chrono::milliseconds(60000));
            if (heartbeat % 20 == 0) {
                heartbeat = 1;
                chm.write_heartbeat();
            }
            heartbeat++;
        }
    });

    while (chm.wait_for_switch() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}

    if (chm.getfancontrol()) {
        while (chm.get_fan_controller_status() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    }
    
    while (chm.set_ps_inh1() == false || chm.set_ps_inh2() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));chm.watch_gpios();}

    int numchecks=0;
    while (chm.update_readings(numchecks) == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));numchecks++;}

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
