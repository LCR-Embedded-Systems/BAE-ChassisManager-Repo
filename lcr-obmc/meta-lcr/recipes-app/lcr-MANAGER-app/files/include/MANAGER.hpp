
#include "../include/global.hpp"


struct ADC_element
{
    std::string name;
    std::string path;
    double V_target;
    int raw_reading;
    double scale_coeff;
    double V_current;
    bool safe;
    ADC_element(const std::string& n, const std::string& p, double target) : name(n), path(p), V_target(target) {}
};

struct gpio_info {
    bool input;
    int chip;
    int line;
    int status;
    std::string name;
    bool dbusflag;
};

class Manager
{

    public:

        Manager();
        ~Manager();
        bool wait_for_switch();
        bool update_readings();
        int get_reading(ADC_element ADC);
        void calculate_cs();
        ADC_element* getSensorByName(const std::string& name);

        bool set_ps();
        bool unset_ps();
        bool set_sr();
        bool set_alarm_gpio();

        nlohmann::json readJson(std::string path);

        void watch_alarms();

        void write_logs_to_flash();

        void init_system_logs();

        void write_log_to_file(std::string log);
        void trim_log();
        void write_heartbeat();

        bool check_temp_service();
        bool check_voltage_service();
        bool check_fan_controller_service();
        bool check_mandatory_sensor_service();
        bool check_gpio_service();
        bool check_ipmi_host_service();

        void watch_services();
        void log_servicechange(bool status);

        void watch_gpios();

        bool get_fan_controller_status();

    private:

        std::vector<std::unique_ptr<ADC_element>> ADCs;

        nlohmann::json limitJson;

        gpio_info sysreset;

        gpio_info ps_enable;

        gpio_info switch_gpio;

        gpio_info alarm_gpio;

        bool temp_service_status;
        bool voltage_service_status;
        bool fan_controller_service_status;
        bool mandatory_sensor_service_status;
        bool gpio_service_status;
        bool ipmi_service_status;

};
