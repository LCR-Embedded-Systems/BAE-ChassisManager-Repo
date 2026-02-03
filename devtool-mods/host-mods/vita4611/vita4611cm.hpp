#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/bus/match.hpp>

#include <iostream>
#include <iomanip>
#include <vector>
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
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Network/EthernetInterface/server.hpp>
#include "ipmid/api-types.hpp"
#include <queue>
#include <vector>
#include <future>


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


#define SHMC_I2C_ADDRESS 0x20
#define SHMC_IPMI_ADDRESS (SHMC_I2C_ADDRESS << 1)

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

/*
* Standard definitions per the specification
*/
#define VITA4611_VSO_IDENTIFIER 3

#define IPMI_GET_DEVICE_ID_CMD 1
#define VITA4611_GETVSOCAPS_CMD 0
#define VITA4611_GET_CHASSIS_ID_CMD 2
#define VITA4611_GET_FRU_LED_PROPS_CMD 5
#define VITA4611_SET_FRU_LED_STATE_CMD 7
#define VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD 0x44

#define IPMI_GET_DEVICE_ID_NETFN 6
#define NETFN_VITA4611 0x2c


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

// ---------------------------------------------------
//  Minimal sensor record (type, number, reading, etc.)
// ---------------------------------------------------
struct SensorRecord
{
    uint8_t  sensorNumber  = 0;      // e.g. 0x20
    VITA4611FRUSensorType  sensorType    = VITA4611FRUSensorType::UNKNOWN;      // e.g. Temperature=01h, Voltage=02h, etc.
    uint8_t  reading       = 0;      // raw reading from Get Sensor Reading
    bool     eventEnabled  = false;  // is event generation on for this sensor?
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
    uint8_t channel;
    
    bool isVITA4611Device;
    bool validDeviceID;
    std::vector<uint8_t> DeviceIDData;
    VSOCaps *vsocaps;

    /*
    * list of all FRUs supported by this device
    * FRU #0 is required by specification
    * Everything else is optional
    */
    std::vector<FruInfo> m_fruList;
    ipmi::RspType<std::vector<uint8_t>> activateFRU(uint8_t fru);
    ipmi::RspType<std::vector<uint8_t>> deactiveFRU(uint8_t fru);
    ipmi::RspType<std::vector<uint8_t>> getFRUState(uint8_t fru);
    ipmi::RspType<std::vector<uint8_t>> getSensorReading(uint8_t fru, uint8_t sensor);
    ipmi::RspType<std::vector<uint8_t>> sendIPMBCommand(uint8_t lun, uint8_t cmd);

    void queryDeviceId();
    void queryVsoCapabilities();
    void discoverFRUs();
    void enumerateFRUSensors(uint8_t fru);

public:
    uint8_t ipmi_address;
    uint8_t hw_address;
    uint8_t site_number;
    uint8_t site_type;
    uint8_t devchannel;
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
    std::vector<uint8_t> getShMCVSOCaps();
    void getAddTable();
    ipmi::RspType<std::vector<uint8_t>> execute_passthrough_cmd(const std::vector<uint8_t>& reqdata);
    VITA4611Device* findDeviceByAddress(uint8_t addr);
    std::vector<uint8_t> handleChMCommand(const std::vector<uint8_t>& reqdata);

private:
    void enumerateI2CDevices();
    void processLoop();
    std::vector<uint8_t> handleRequest(const VITA4611Command& req);

    std::thread worker;
    std::queue<VITA4611Command> requestQueue;
    std::mutex queueMutex;
    std::condition_variable queueCond;

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
};



