#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/bus/match.hpp>
#include <boost/asio/steady_timer.hpp>

#include <iostream>
#include <iomanip>
#include <map>
#include <optional>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ipmid/api.hpp>
#include <ipmid/utils.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Network/EthernetInterface/server.hpp>
#include "ipmid/api-types.hpp"
#include <queue>
#include <future>

#include <sdbusplus/server/object.hpp>
#include <systemd/sd-bus.h>

#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

#include "config.h"

#include "user_channel/channel_layer.hpp"
    
#include <bitset>
#include <cmath>
#include <fstream>
#include <variant>
#include <vector>
#include <string>

#include <fstream>
#include <ostream>

#define CHMC_I2C_ADDRESS 0x20
#define CHMC_IPMI_ADDRESS (CHMC_I2C_ADDRESS << 1)

#define DEVICE_ID 0x55
#define DEVICE_REVISION 0x01
#define FIRMWARE_REVISION_1 0x01
#define FIRMWARE_REVISION_2 0x01
#define IPMI_VERSION 0x01
#define ADDITIONAL_DEVICE_SUPPORT 0x01
#define MANUFACTURER_ID_1 0x01
#define MANUFACTURER_ID_2 0x01
#define PRODUCT_ID_1 0x01
#define PRODUCT_ID_2 0x01

/*
* Ipmitool commands. These are registered as
* oem commands under netfn 0x3A
* All of these go to the sbc (caller has to provide
* i2c address)
*/
#define IPMITOOL_CMD_GET_DEVICE_ID 1
#define IPMITOOL_CMD_SET_DEVICE_POWER 2
#define IPMITOOL_CMD_GET_DEVICE_POWER_STATE 3
#define IPMITOOL_CMD_QUERY_TEMPERATURE 4
#define IPMITOOL_CMD_QUERY_POWER 5
#define IPMITOOL_CMD_GET_FW_UPGRADE_STATUS 6
#define IPMITOOL_CMD_QUERY_CARD_STATE 7
#define IPMITOOL_CMD_GET_VSO_CAPS   8
#define IPMITOOL_CMD_GET_SDR_INFO   0x20
#define IPMITOOL_CMD_GET_SENSOR_VALUE   0x2d
#define IPMITOOL_CMD_GET_SDR   0x21
#define IPMITOOL_CMD_RESERVE_SDR_REPO   0x22


#define IPMI_GET_DEVICE_ID_CMD 1
#define IPMI_GET_SENSOR_READING_CMD 0x2d
/*
* Standard definitions per the specification
*/
#define VITA4611_VSO_IDENTIFIER 3

//Mandatory Command Definitions
#define VITA4611_GETVSOCAPS_CMD 0
#define VITA4611_GET_CHASSIS_ADDRESS_TABLE_INFO_CMD 0x01
#define VITA4611_GET_CHASSIS_ID_CMD 0x02
#define VITA4611_SET_CHASSIS_ID_CMD 0x03
#define VITA4611_FRU_CONTROL 0x04
#define VITA4611_GET_FRU_LED_PROPS_CMD 0x05
#define VITA4611_GET_FRU_LED_COLOR_CAPS_CMD 0x06
#define VITA4611_SET_FRU_LED_STATE_CMD 0x07
#define VITA4611_GET_FRU_LED_STATE_CMD 0x08
#define VITA4611_SET_IPMB_STATE_CMD 0x09
#define VITA4611_SET_FRU_STATE_POLICY_BITS_CMD 0x0A
#define VITA4611_GET_FRU_STATE_POLICY_BITS_CMD 0x0B
#define VITA4611_SET_FRU_ACTIVATION_CMD 0x0C
#define VITA4611_GET_DEVICE_LOCATOR_RECORD_ID_CMD 0x0D

#define VITA4611_GET_FAN_SPEED_PROPERTIES_CMD 0x14
#define VITA4611_SET_FAN_LEVEL_CMD 0x15
#define VITA4611_GET_FAN_LEVEL_CMD 0x16

#define VITA4611_GET_CHASSIS_MANAGER_IPMB_ADDRESS_CMD 0x1B
#define VITA4611_SET_FAN_POLICY_CMD 0x1C
#define VITA4611_GET_FAN_POLICY_CMD 0x1D
#define VITA4611_FRU_CONTROL_CAPABILITIES_CMD 0x1E
#define VITA4611_FRU_INVENTORY_DEVICE_LOCK_CONTROL_CMD 0x1F
#define VITA4611_FRU_INVENTORY_DEVICE_WRITE_CMD 0x20
#define VITA4611_CHASSIS_MANAGER_IP_ADDRESSES_CMD 0x21
#define VITA4611_GET_FRU_ADDRESS_INFO_CMD 0x40
#define VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD 0x44

#define NETFN_APP 6
#define NETFN_CHASSIS 0
#define NETFN_S_E 4
#define NETFN_STORAGE 0x0a
#define NETFN_TRANSPORT 0x0c
#define NETFN_VITA4611 0x2c

#define IPMI_COMPLETION_CODE 0x01

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface = sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;

extern std::ofstream dbgFile;

/*
# of bytes in the response for get fru led properties
* command (successful)
* See the VITA 46.11 specification 
* 0 byte is 3
* 1 byte is reserved
* 2 byte is # of LED
*/
constexpr int GET_FRU_LED_RESPONSE_DATA_SIZE = 3;
constexpr int GET_MANDATORY_SENSOR_NUMBERS_RESPONSE_SIZE = 9; /* including the vso id */

//std::map<uint8_t, uint8_t> channel_add_map;

struct CAT_entry
{
    uint8_t hw_address;
    uint8_t site_num;
    uint8_t site_type;
};

struct CAT
{
    uint8_t record_type_id;
    uint8_t version;
    uint8_t record_length;
    uint8_t record_checksum;
    uint8_t header_checksum;
    std::array<uint8_t, 3> manufacturer_id;
    uint8_t vita_record_id;
    uint8_t record_format_version;
    uint8_t chassis_id_type;
    std::array<uint8_t, 20> chassis_id_field;
    uint8_t chassis_add_table_entries_count;
    std::vector<CAT_entry> entries;
};

struct VSOCaps
{
    uint8_t VSOIdentifier;
    uint8_t IPMCIdentifier;
    uint8_t IPMBCapabilities;
    uint8_t VSOStandard;
    uint8_t VSORevision;
    uint8_t MaxFRUDeviceID;
    uint8_t IPMCFRUDeviceID;
};

struct FRUSensorInfo
{
    uint8_t FRUStateSensorNumber;
    uint8_t FRUHealthSensorNumber;
    uint8_t FRUVoltageSensorNumber;

    uint8_t FRUTempSensorNumber;
    uint8_t PayloadTestResultsSensorNumber;
    uint8_t PayloadTestStatusSensorNumber;
    uint8_t Reserved;
    uint8_t PayloadModeSensorNumber;
};

enum class FruState : uint8_t {
    M0_IPMCInactive           = 0,
    M1_FRUInactive            = 1,
    // M2 / M3 not used in VITA 46.11
    M4_FRUActive              = 4,
    M5_FRUDeactivationRequest = 5,
    M6_FRUDeactivationInProg  = 6,
    M7_FRUCommLost            = 7,
    UNKNOWN                   = 0xFF
};

enum class VITA4611FRUSensorType : uint8_t {
    FRUSensor_Type_State = 1,
    FRUSensor_Type_Health = 2,
    FRUSensor_Type_Voltage = 3,
    FRUSensor_Type_Temp = 4,
    FRUSensor_Type_PayloadTestResults = 5,
    FRUSensor_Type_PayloadTestStatus = 6,
    FRUSensor_Type_PayloadModeSensor = 7,
    UNKNOWN = 0xFF
};

struct SensorRecord
{
    uint8_t record_type;
    uint8_t  sensorNumber  = 0;      // e.g. 0x20
    VITA4611FRUSensorType  sensorType    = VITA4611FRUSensorType::UNKNOWN;      // e.g. Temperature=01h, Voltage=02h, etc.
    uint8_t  raw_reading       = 0;      // raw reading from Get Sensor Reading
    uint8_t  reading       = 0;
    double   reading_converted;
    bool     eventEnabled  = false;  // is event generation on for this sensor?
    std::vector<uint8_t> sdr_header_raw;
    std::vector<uint8_t> sdr_info_raw;
    std::string SDR_Name;
    std::string SDR_Units;
    std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>> sensorIface;


    uint8_t sensor_caps;
    uint8_t sensor_type;
    uint8_t sensor_units_1;
    uint8_t sensor_units_3;
    uint8_t sensor_linearization;
    uint8_t sensor_M;
    uint8_t sensor_Mtol;
    uint8_t sensor_B;
    uint8_t sensor_Bacc;
    uint8_t sensor_Acc;
    uint8_t R_exp;
    uint8_t B_exp;
    uint8_t analog_characteristics;
    uint8_t normal_maximum;
    uint8_t normal_minimum;
    uint8_t sensor_maximum;
    uint8_t sensor_minimum;
    uint8_t upper_nonrec_threshold;
    uint8_t lower_nonrec_threshold;
    uint8_t upper_critical_threshold;
    uint8_t lower_critical_threshold;
    uint8_t upper_noncritical_threshold;
    uint8_t lower_noncritical_threshold;
    double converted_upper_nonrec_threshold;
    double converted_lower_nonrec_threshold;
    double converted_upper_critical_threshold;
    double converted_lower_critical_threshold;
    double converted_upper_noncritical_threshold;
    double converted_lower_noncritical_threshold;
};

// ---------------------------------------------------
//  FRU Info: Holds data about a single FRU instance
// ---------------------------------------------------
struct FruInfo
{
    uint8_t         deviceId     = 0;         // 0 for IFRU, 1..n for Subsidiary
    FruState        state        = FruState::UNKNOWN;
    std::string     fruName;                  // e.g. "Slot 3 RTM"

    // Minimal sensor list; real code might store a full IPMI SDR
    std::vector<SensorRecord> sensors;
};

/*
VITA4611DeviceInfo
Object that represents a vita4611 device
we keep track of

channel :- Bus for the device
isVITA4611Device :- indicates if the device is a vita 46.11
deviceIdResponse :- Data bytes of the get device id
vsocaps :- If the device is a vita 46.11,
  then this is valid and contains the vso capabilities
  of the device

  fru_led_info :- For each FRU, the number of LEDs
  implemented by that FRU
*/
class VITA4611Device {

private:
    std::vector<uint8_t> sensorPopulationChangeIndicatorCache;
    uint8_t dynamic_sensors;
    uint8_t channel;
    uint8_t device_record_total;
    std::vector<uint8_t> mandatorySensorNumbers;
    bool isVITA4611Device;
    bool validDeviceID;
    std::vector<uint8_t> DeviceIDData;
    VSOCaps *vsocaps;

    /*
    * list of all FRUs supported by this device
    * FRU #0 is required by specification
    * Everything else is optional
    */
    
    // std::vector<FruInfo> frus;
    ipmi::RspType<std::vector<uint8_t>> activateFRU(uint8_t fru);
    ipmi::RspType<std::vector<uint8_t>> deactiveFRU(uint8_t fru);
    ipmi::RspType<std::vector<uint8_t>> getFRUState(uint8_t fru);
    void getSensorReading(SensorRecord& sensor, uint8_t channel);
    std::tuple<uint8_t, uint8_t> reserveSDRRepo();
    std::vector<uint8_t> getSDRHeader(std::tuple<uint8_t, uint8_t> reservation_id, uint8_t sensor_number);
    std::vector<uint8_t> getSDRVals(std::tuple<uint8_t, uint8_t> reservation_id, uint8_t sensor_number, int length);

    void queryDeviceId(uint8_t addr);
    void queryVsoCapabilities();
    void discoverFRUs();
    void enumerateFRUSensors(uint8_t fru, uint8_t addr);
    void handleCompactSdr(std::vector<uint8_t> sdrheader, std::vector<uint8_t> sdrvals);
    void handleFullSdr(std::vector<uint8_t> sdrheader, std::vector<uint8_t> sdrvals);

    bool isSensorDiscovered(uint8_t sensorNum);
    SensorRecord* getSensorRecord(uint8_t sensorNum);
    double convert_sensor_reading(uint8_t units_def, uint8_t linearization, uint8_t M, uint8_t Mtol, uint8_t B, uint8_t Bacc, 
        uint8_t Acc, uint8_t R_exp, uint8_t B_exp, uint8_t analog_characteristics, uint8_t reading);

public:
    FruInfo fruinfo;
    void getAllSensorReadings();
    void getAllSDRs();
    void checkSDRInfo();
    uint8_t get_dynamic_sensors();
    uint8_t ipmi_address;
    uint8_t hw_address;
    uint8_t site_number;
    uint8_t site_type;
    uint8_t devchannel;
    uint8_t device_id;

    uint8_t fru_activation_policy;
    uint8_t fru_activation_flag;

    VITA4611Device(uint8_t channel, uint8_t ipmi_address);
    ~VITA4611Device();
};

/*
* I2CDeviceInfo represents a i2c device
*/
struct I2CDeviceInfo {
    int i2cbus;
    uint8_t devicei2caddress;
    int channel;
    uint8_t ShMCi2caddress;
};

enum class VITA4611CMCommandType
{
    IPMI,
    Shutdown
};

struct VITA4611Command
{
    VITA4611CMCommandType type = VITA4611CMCommandType::IPMI;

    std::promise<std::vector<uint8_t>> promise;

    uint8_t netFn = 0;
    uint8_t cmd = 0;
    uint8_t lun = 0;
    std::vector<uint8_t> payload;
    uint8_t targetAddr = 0;
};

/*
* VITA4611_ChassisManager
* Models a vita 4611 chassis manager
*/
class VITA4611_ChassisManager
{
public:
    VITA4611_ChassisManager();
    ~VITA4611_ChassisManager();

    std::future<std::vector<uint8_t>> submit(VITA4611Command req);
    void sendShutdown();

    void get_device_list(std::vector<uint8_t>& ipmi_address_list);

    CAT chassisAddressTable;

    uint8_t m_ipmb_a_enable;
    uint8_t m_ipmb_b_enable;
    uint8_t m_ipmb_a_Link_ID;
    uint8_t m_ipmb_b_Link_ID;

    //Helper functions for command responses

    //Mandatory Vita commands
    std::vector<uint8_t> getShMCVSOCaps(); //done
    std::vector<uint8_t> getAddTable(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> getChassisIdentifier(); //done
    std::vector<uint8_t> setChassisIdentifier(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> fruControl(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> setIpmbState(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> setFruStatePolicyBits(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> getFruStatePolicyBits(uint8_t devId); //done
    std::vector<uint8_t> setFruActivation(const std::vector<uint8_t>& reqdata); //done
    std::vector<uint8_t> getDeviceLocatorRecordId(const std::vector<uint8_t>& reqdata);
    std::vector<uint8_t> getChassisManagerIpmbAddress(); //done

    std::vector<uint8_t> setFanPolicy(const std::vector<uint8_t>& reqdata); // partially done
    std::vector<uint8_t> getFanPolicy(const std::vector<uint8_t>& reqdata); // done

    std::vector<uint8_t> fruControlCapabilities();
    std::vector<uint8_t> fruInventoryDeviceLockControl();
    std::vector<uint8_t> fruInventoryDeviceWrite();

    std::vector<uint8_t> getChassisManagerIpAddresses(uint8_t addr_num); //done

    //Helper function
    std::vector<uint8_t> get_chm_fru_info();

    std::vector<uint8_t> getFruAddressInfo(const std::vector<uint8_t>& reqdata);
    std::vector<uint8_t> getMandatorySensorNumbers();

    //Mandatory LAN Device Commands
    std::vector<uint8_t> setLanConfigurationParams();
    std::vector<uint8_t> getLanConfigurationParams();

    //Mandatory Fan Commands
    std::vector<uint8_t> getFanSpeedProperties(const std::vector<uint8_t>& reqdata); // done
    std::vector<uint8_t> getFanLevel(const std::vector<uint8_t>& reqdata); // done; TODO: make byte 5 reflect status of fan controller service
    std::vector<uint8_t> setFanLevel(const std::vector<uint8_t>& reqdata);

    ipmi::RspType<std::vector<uint8_t>> execute_passthrough_cmd(const std::vector<uint8_t>& reqdata);
    VITA4611Device* findDeviceByAddress(uint8_t addr);
    VITA4611Device* findDeviceByPhysAddress(uint8_t phys_addr);
    VITA4611Device* findDeviceByHwAddress(uint8_t hw_addr);
    VITA4611Device* getDeviceByID(uint8_t id);
    std::vector<uint8_t> handleChMCommand(const std::vector<uint8_t>& reqdata);

    int pollDeviceSensors();
    int pollSDRs();

    void exposeFruSensors();


private:
    uint8_t deviceId;
    uint8_t deviceRevision;
    uint8_t firmwareRevision1;
    uint8_t firmwareRevision2;
    uint8_t IPMIVersion;
    uint8_t additionalDeviceSupport;
    uint8_t manufacturerID1;
    uint8_t manufacturerID2;
    uint8_t productID1;
    uint8_t productID2;
    void enumerateI2CDevices();
    void processLoop();
    std::vector<uint8_t> handleRequest(const VITA4611Command& req);

    uint8_t ipmb_address;

    uint8_t address_count;
    uint8_t site_type;
    uint8_t site_number;
    uint8_t max_unavailable_time;
    uint8_t address_type;
    std::vector<uint8_t> ip_address;


    std::thread worker;
    std::queue<VITA4611Command> requestQueue;
    std::mutex queueMutex;
    std::condition_variable queueCond;

    std::shared_ptr<boost::asio::steady_timer> fanRestartTimer;

    /*
    * i2cdeviceMap is the map of all devices discovered
    * on a given bus
    */
    std::map<int, std::vector<I2CDeviceInfo>> i2cdeviceMap;
    /*
    * devicMap is a map of all devices that are compliant
    * with vita46.11 discovered on the bus
    */
    std::map<uint8_t, VITA4611Device *> deviceMap;

    bool verify_ipmi_address_is_valid(const uint8_t ipmi_address);
    sdbusplus::bus::bus bus;

    std::map<std::string, std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>> sensorIfaces;
};



