#include "vita4611cm.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <ostream>

#include <stdio.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
using namespace phosphor::logging;
typedef std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>> ipmb_response_return_type_t;


//////////////////////////////////////////////////

/*

    NECESSARY GLOBAL FUNCTIONS

*/

//////////////////////////////////////////////////


/*
*
* Functions taken from sdbusplus.hpp from the phosphor-fan-presence app
*
*/

namespace phosphor
{
namespace interface
{
namespace util
{
namespace detail
{
namespace errors = sdbusplus::xyz::openbmc_project::Common::Error;
} // namespace detail


/**
 * @class DBusError
 *
 * The base class for the exceptions thrown on fails in the various
 * SDBusPlus calls.  Used so that a single catch statement can catch
 * any type of these exceptions.
 *
 * None of these exceptions will log anything when they are created,
 * it is up to the handler to do that if desired.
 */
class DBusError : public std::runtime_error
{
  public:
    explicit DBusError(const std::string& msg) : std::runtime_error(msg)
    {}
};

/**
 * @class DBusMethodError
 *
 * Thrown on a DBus Method call failure
 */
class DBusMethodError : public DBusError
{
  public:
    DBusMethodError(const std::string& busName, const std::string& path,
                    const std::string& interface, const std::string& method) :
        DBusError("DBus method failed"),
        busName(busName), path(path), interface(interface), method(method)
    {}

    const std::string busName;
    const std::string path;
    const std::string interface;
    const std::string method;
};

/**
 * @class DBusServiceError
 *
 * Thrown when a service lookup fails.  Usually this points to
 * the object path not being present in D-Bus.
 */
class DBusServiceError : public DBusError
{
  public:
    DBusServiceError(const std::string& path, const std::string& interface) :
        DBusError(
            "DBus service lookup failed: {} {}"),
        path(path), interface(interface)
    {}

    const std::string path;
    const std::string interface;
};


/**
 * @class DBusPropertyError
 *
 * Thrown when a set/get property fails.
 */
class DBusPropertyError : public DBusError
{
  public:
    DBusPropertyError(const std::string& msg, const std::string& busName,
                      const std::string& path, const std::string& interface,
                      const std::string& property) :
        DBusError(msg),
        busName(busName), path(path), interface(interface), property(property)
    {}

    const std::string busName;
    const std::string path;
    const std::string interface;
    const std::string property;
};

/** @brief Get the bus connection. */
static auto& getBus() __attribute__((pure));

static auto& getBus()
{
    static auto bus = sdbusplus::bus::new_default();
    return bus;
}

/** @brief Alias for PropertiesChanged signal callbacks. */
template <typename... T>
using Properties = std::map<std::string, std::variant<T...>>;

template <typename... Args>
static auto callMethod(sdbusplus::bus_t& bus, const std::string& busName,
                       const std::string& path,
                       const std::string& interface,
                       const std::string& method, Args&&... args)
{

    auto reqMsg = bus.new_method_call(busName.c_str(), path.c_str(),
                                      interface.c_str(), method.c_str());
    reqMsg.append(std::forward<Args>(args)...);
    try
    {
        auto respMsg = bus.call(reqMsg);
        if (respMsg.is_method_error())
        {
            throw DBusMethodError{busName, path, interface, method};
        }
        return respMsg;
    }
    catch (const sdbusplus::exception_t&)
    {
        throw DBusMethodError{busName, path, interface, method};
    }
}

template <typename Ret, typename... Args>
static auto
callMethodAndRead(sdbusplus::bus_t& bus, const std::string& busName,
                  const std::string& path, const std::string& interface,
                  const std::string& method, Args&&... args)
{
    sdbusplus::message_t respMsg = callMethod<Args...>(
        bus, busName, path, interface, method, std::forward<Args>(args)...);
    Ret resp;
    respMsg.read(resp);
    return resp;
}
/** @brief Invoke a method and read the response. */
template <typename Ret, typename... Args>
static auto callMethodAndRead(const std::string& busName,
                                const std::string& path,
                                const std::string& interface,
                                const std::string& method, Args&&... args)
{
    return callMethodAndRead<Ret>(getBus(), busName, path, interface,
                                    method, std::forward<Args>(args)...);
}

/** @brief Get subtree paths from the mapper without checking response. */
// static auto getSubTreePathsRaw(sdbusplus::bus_t& bus,
//     const std::string& path,
//     const std::string& interface, int32_t depth)
// {
//     using namespace std::literals::string_literals;

//     using Path = std::string;
//     using Intf = std::string;
//     using Intfs = std::vector<Intf>;
//     using ObjectPaths = std::vector<Path>;
//     Intfs intfs = {interface};

//     return callMethodAndRead<ObjectPaths>(
//         bus, "xyz.openbmc_project.ObjectMapper"s,
//         "/xyz/openbmc_project/object_mapper"s,
//         "xyz.openbmc_project.ObjectMapper"s, "GetSubTreePaths"s, path,
//         depth, intfs);
// }

// /** @brief Get subtree paths from the mapper. */
// static auto getSubTreePaths(sdbusplus::bus_t& bus, const std::string& path,
//      const std::string& interface, int32_t depth)
// {
//     auto mapperResp = getSubTreePathsRaw(bus, path, interface, depth);
//     if (mapperResp.empty())
//     {
//         phosphor::logging::log<phosphor::logging::level::ERR>(
//             "Empty response from mapper GetSubTreePaths",
//             phosphor::logging::entry("SUBTREE=%s", path.c_str()),
//             phosphor::logging::entry("INTERFACE=%s", interface.c_str()),
//             phosphor::logging::entry("DEPTH=%u", depth));
//             phosphor::logging::elog<detail::errors::InternalFailure>();
//     }
//     return mapperResp;
// }



// /** @brief Get subtree from the mapper without checking response. */
// static auto getSubTreeRaw(sdbusplus::bus_t& bus, const std::string& path,
//     const std::string& interface, int32_t depth)
// {
//     using namespace std::literals::string_literals;

//     using Path = std::string;
//     using Intf = std::string;
//     using Serv = std::string;
//     using Intfs = std::vector<Intf>;
//     using Objects = std::map<Path, std::map<Serv, Intfs>>;
//     Intfs intfs = {interface};

//     return callMethodAndRead<Objects>(bus,
//                         "xyz.openbmc_project.ObjectMapper"s,
//                         "/xyz/openbmc_project/object_mapper"s,
//                         "xyz.openbmc_project.ObjectMapper"s,
//                         "GetSubTree"s, path, depth, intfs);
// }

/** @brief Get service from the mapper without checking response. */
static auto getServiceRaw(sdbusplus::bus_t& bus, const std::string& path,
    const std::string& interface)
{
    using namespace std::literals::string_literals;
    using GetObject = std::map<std::string, std::vector<std::string>>;

    return callMethodAndRead<GetObject>(
        bus, "xyz.openbmc_project.ObjectMapper"s,
        "/xyz/openbmc_project/object_mapper"s,
        "xyz.openbmc_project.ObjectMapper"s, "GetObject"s, path,
        GetObject::mapped_type{interface});
}

/** @brief Get service from the mapper. */
static auto getService(sdbusplus::bus_t& bus, const std::string& path,
    const std::string& interface)
{
    try
    {
        auto mapperResp = getServiceRaw(bus, path, interface);

        if (mapperResp.empty())
        {
            // Should never happen.  A missing object would fail
            // in callMethodAndRead()
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Empty mapper response on service lookup");
            throw DBusServiceError{path, interface};
        }
        return mapperResp.begin()->first;
    }
    catch (const DBusMethodError& e)
    {
        throw DBusServiceError{path, interface};
    }
}

/** @brief Get a property with mapper lookup. */
template <typename Property>
static auto getProperty(sdbusplus::bus_t& bus, const std::string& path,
                        const std::string& interface,
                        const std::string& property)
{
    using namespace std::literals::string_literals;

    auto service = getService(bus, path, interface);
    auto msg =
        callMethod(bus, service, path, "org.freedesktop.DBus.Properties"s,
                    "Get"s, interface, property);
    if (msg.is_method_error())
    {
        throw DBusPropertyError{"DBus get property failed", service, path,
                                interface, property};
    }
    std::variant<Property> value;
    msg.read(value);
    return std::get<Property>(value);
}


/** @brief Invoke a method and return without checking for error. */
template <typename... Args>
static auto callMethodAndReturn(sdbusplus::bus_t& bus,
                                const std::string& busName,
                                const std::string& path,
                                const std::string& interface,
                                const std::string& method, Args&&... args)
{
    auto reqMsg = bus.new_method_call(busName.c_str(), path.c_str(),
                                        interface.c_str(), method.c_str());
    reqMsg.append(std::forward<Args>(args)...);
    auto respMsg = bus.call(reqMsg);

    return respMsg;
}

/** @brief Set a property with mapper lookup. */
template <typename Property>
static void setProperty(sdbusplus::bus_t& bus, const std::string& path,
                        const std::string& interface,
                        const std::string& property, Property&& value)
{
    using namespace std::literals::string_literals;

    std::variant<Property> varValue(std::forward<Property>(value));

    auto service = getService(bus, path, interface);
    auto msg = callMethodAndReturn(bus, service, path,
                                    "org.freedesktop.DBus.Properties"s,
                                    "Set"s, interface, property, varValue);
    if (msg.is_method_error())
    {
        throw DBusPropertyError{"DBus set property failed", service, path,
                                interface, property};
    }
}

}
}
}

//////////////////////////////////////////////////

/*

    NECESSARY GLOBAL FUNCTIONS END

*/

//////////////////////////////////////////////////

// Helper function to check if a systemd service is active
bool isServiceActive(const std::string& serviceName) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *msg = NULL;
    sd_bus *bus = NULL;
    char *state;
    bool isActive = false;
    int r;

    // Connect to the system bus
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto finish;
    }

    // Call GetUnit method to get the service unit
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "GetUnit",
                          &error,
                          &msg,
                          "s",
                          serviceName.c_str());
    
    if (r < 0) {
        fprintf(stderr, "Failed to get unit: %s\n", error.message);
        goto finish;
    }

    const char *path;
    r = sd_bus_message_read(msg, "o", &path);
    if (r < 0) {
        fprintf(stderr, "Failed to parse response: %s\n", strerror(-r));
        goto finish;
    }

    sd_bus_message_unref(msg);
    msg = NULL;

    // Get the ActiveState property
    r = sd_bus_get_property_string(bus,
                                   "org.freedesktop.systemd1",
                                   path,
                                   "org.freedesktop.systemd1.Unit",
                                   "ActiveState",
                                   &error,
                                   &state);
    
    if (r < 0) {
        fprintf(stderr, "Failed to get ActiveState: %s\n", error.message);
        goto finish;
    }

    isActive = (strcmp(state, "active") == 0);

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(msg);
    sd_bus_unref(bus);
    
    return isActive;
}




std::map<int, std::string> sensorUnitTypeCodes = {
    {0, "unknown"},
    {1, "degrees C"},
    {2, "degrees F"},
    {3, "degrees K"},
    {4, "Volts"},
    {5, "Amps"},
    {6, "Watts"},
    {7, "Joules"},
    {8, "Coulombs"},
    {9, "VA"},
    {10, "Nits"}
};




std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

VITA4611Device* VITA4611_ChassisManager::findDeviceByAddress(uint8_t addr) {
    for (const auto& [key, dev] : this->deviceMap) {
        if (dev && dev->ipmi_address == addr) {
            return dev;
        }
    }
    return nullptr;
}

VITA4611Device* VITA4611_ChassisManager::findDeviceByPhysAddress(uint8_t phys_addr) {
    for (const auto& [key, dev] : this->deviceMap) {
        if (dev && dev->site_number == phys_addr) {
            return dev;
        }
    }
    return nullptr;
}

VITA4611Device* VITA4611_ChassisManager::findDeviceByHwAddress(uint8_t hw_addr) {
    for (const auto& [key, dev] : this->deviceMap) {
        if (dev && dev->hw_address == hw_addr) {
            return dev;
        }
    }
    return nullptr;
}

VITA4611Device* VITA4611_ChassisManager::getDeviceByID(uint8_t id) {
    for (const auto& [key, dev] : this->deviceMap) {
        if (dev && dev->device_id == id) {
            return dev;
        }
    }
    return nullptr;
}

/*
* sendIPMBRequest
* This function sends a IPMI request over i2c bus
* using the d-bus method sendRequest provided by ipmbbridge
* Channel :- Channel initialized by ipmbbridged based on its
        JSON script

* netfn , lun , cmd are copied in the IPMI request on the i2c bus
* data (optional depends on above) is copied to IPMI request
*
* If the response fails (or timesout ) nullopt is returned. 
* caller can optionally look at the cc value that is returned
* as an output parameter
*/
std::optional<ipmb_response_return_type_t> sendIPMBRequest(
    const uint8_t channel,
    const uint8_t addr,
    const uint8_t netfn, 
    const uint8_t lun, 
    const uint8_t cmd, 
    std::vector<uint8_t> &data,
    uint8_t *ipmi_response_cc, /* out */
    bool verbose
)
{
    if (verbose) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID: Sending IPMB request with the following data: " << std::endl;
        dbgFile << "\t channel: " << static_cast<int>(channel) << std::endl;
        dbgFile << "\t address: " << static_cast<int>(addr) << std::endl;
        dbgFile << "\t netfn: " << static_cast<int>(netfn) << std::endl;
        dbgFile << "\t lun: " << static_cast<int>(lun) << std::endl;
        dbgFile << "\t cmd: " << static_cast<int>(cmd) << std::endl;
        dbgFile << "\t data: ";
        for (const auto& byte : data)
        {
            dbgFile << "0x" << std::hex << std::uppercase << static_cast<int>(byte) << " ";
        }
        dbgFile << std::dec << std::nouppercase << std::endl; // Reset stream formatting

        if (channel == 4) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID: using ipmb b " << std::endl;
        }
        else if (channel == 0) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID: using ipmb a" << std::endl;
        }
    }
    

    try
    {
        auto bus = sdbusplus::bus::new_default();

        // Prepare the DBus method call
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Ipmi.Channel.Ipmb",       // Service
            "/xyz/openbmc_project/Ipmi/Channel/Ipmb",      // Object path
            "org.openbmc.Ipmb",                            // Interface
            "sendRequest");                                // Method

        // Append parameters
        method.append(channel, addr, netfn, lun, cmd, data);

        // Call the method synchronously
        auto reply = bus.call(method);

        if (reply.is_method_error())
        {
            dbgFile << "LCR:IPMID:Test() DBus method call failed (sendRequest)." << std::endl;
            return std::nullopt;
        }

        std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>> result;
        uint8_t cc;

        reply.read(result);
        cc = std::get<4>(result);

        if (!cc) {
            // success
            *ipmi_response_cc = cc;
            return result;
        }
        else {
            *ipmi_response_cc = cc;
            return std::nullopt;
        }
    }
    catch (const std::exception& e)
    {
        dbgFile << "LCR::IPMID:GetDeviceID::Exception: " << e.what() << std::endl;
        *ipmi_response_cc = 0xFF; /* TODO revisit this later */
        return std::nullopt;
    }

    /* We should never come here since the try catch should catch everything */
    *ipmi_response_cc = 0xFF;
    return std::nullopt;
}

void
VITA4611Device::queryDeviceId(uint8_t addr)
{
    std::optional<ipmb_response_return_type_t> ipmb_response;
    std::vector<uint8_t> payload; // Get Device ID has no payload
    uint8_t ipmi_response_cc;
    ipmb_response = sendIPMBRequest(
        0,
        addr, //TODO make this generalized
        NETFN_APP,
        0,
        IPMI_GET_DEVICE_ID_CMD,
        payload,
        &ipmi_response_cc,
        false
    );
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp;
        resp = ipmb_response.value();

        DeviceIDData = std::get<5>(resp);
        validDeviceID = true;
    }
    else {
        validDeviceID = false;
    }
}

void
VITA4611Device::enumerateFRUSensors(uint8_t fru, uint8_t addr)
{
    std::optional<ipmb_response_return_type_t> ipmb_response;
    std::optional<ipmb_response_return_type_t> ipmb_response_sdr_repo_info;
    std::optional<ipmb_response_return_type_t> ipmb_response_sdr_info;
    uint8_t ipmi_response_cc;
    std::vector<uint8_t> payload = {0x03, (uint8_t) fru}; // VSO Group Extension Identifier
    std::vector<uint8_t> emptypayload;
    SensorRecord rec;
    ipmb_response = 
        sendIPMBRequest(
            0,
            addr,
            NETFN_VITA4611,
            0,
            VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD,
            payload,
            &ipmi_response_cc, 
            false);
    
    ipmb_response_sdr_info = 
        sendIPMBRequest(
            0,
            addr,
            NETFN_S_E,
            0,
            IPMITOOL_CMD_GET_SDR_INFO,
            emptypayload,
            &ipmi_response_cc,
            false);

    ipmb_response_sdr_repo_info = 
        sendIPMBRequest(
            0,
            addr,
            NETFN_STORAGE,
            0,
            IPMITOOL_CMD_GET_SDR_INFO,
            emptypayload,
            &ipmi_response_cc,
            false);
    
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp;
        resp = ipmb_response.value();
        auto resp_payload = std::get<5>(resp);

        /* verify that the first byte is vso identifier
        *  and the length is 9 bytes
        */
        if ((resp_payload[0] != VITA4611_VSO_IDENTIFIER))
                return;

        /*
        * Refer to VITA 46.11 specification
        * Table 10.1.3.26-1
        */
       fruinfo.deviceId = fru;
       fruinfo.fruName = "VITA4611FRU";

       rec.sensorNumber = resp_payload[1];
       mandatorySensorNumbers.push_back(resp_payload[1]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_State;
       fruinfo.sensors.push_back(std::move(rec));

       rec.sensorNumber = resp_payload[2];
       mandatorySensorNumbers.push_back(resp_payload[2]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Health;
       fruinfo.sensors.push_back(std::move(rec));

       rec.sensorNumber = resp_payload[3];
       mandatorySensorNumbers.push_back(resp_payload[3]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Voltage;
       fruinfo.sensors.push_back(std::move(rec));

       rec.sensorNumber = resp_payload[4];
       mandatorySensorNumbers.push_back(resp_payload[4]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Temp;
       fruinfo.sensors.push_back(std::move(rec));

       rec.sensorNumber = resp_payload[5];
       mandatorySensorNumbers.push_back(resp_payload[5]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_PayloadTestResults;
       fruinfo.sensors.push_back(std::move(rec));

       rec.sensorNumber = resp_payload[6];
       mandatorySensorNumbers.push_back(resp_payload[6]);
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_PayloadTestStatus;
       fruinfo.sensors.push_back(std::move(rec));

    }
    if (ipmb_response_sdr_repo_info.has_value()) {
        ipmb_response_return_type_t resp_sdr_repo_info;
        resp_sdr_repo_info = ipmb_response_sdr_repo_info.value();
        auto resp_payload_sdr_repo_info = std::get<5>(resp_sdr_repo_info);
        uint8_t num_records = resp_payload_sdr_repo_info[1];
        device_record_total = num_records;
        // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID total number of records: " << static_cast<int>(device_record_total) << std::endl;
    } else {
        // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID No response from sdr repo info for device at " << static_cast<int>(addr) << std::endl;
    }

    if (ipmb_response_sdr_info.has_value()) {
        ipmb_response_return_type_t resp_sdr_info;
        resp_sdr_info = ipmb_response_sdr_info.value();
        auto resp_payload_sdr_info = std::get<5>(resp_sdr_info);
        dynamic_sensors = resp_payload_sdr_info[1] >> 7;
    } else {
        // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID No response from sdr info (NOT REPO) for device at " << static_cast<int>(addr) << std::endl;
    }

    getAllSDRs();

}

void VITA4611Device::getSensorReading(SensorRecord& sensor, uint8_t channel) {
    std::optional<ipmb_response_return_type_t> ipmb_response;
    uint8_t sensorNumber = sensor.sensorNumber;
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. getSensorReading at sensor number " << static_cast<int>(sensorNumber) << " at address " << static_cast<int>(ipmi_address) << std::endl;
    uint8_t ipmi_response_cc;
    std::vector<uint8_t> payload;
    payload.push_back(sensorNumber);
    ipmb_response = sendIPMBRequest(
        channel,
        ipmi_address,
        NETFN_S_E,
        0,
        IPMI_GET_SENSOR_READING_CMD,
        payload,
        &ipmi_response_cc,
        false
    );
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp = ipmb_response.value();
        std::vector<uint8_t> data = std::get<5>(resp);
        sensor.raw_reading = data[0];
    } else {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Error: No valid response from sendIPMBRequest for sensor number " << static_cast<int>(sensorNumber) << std::endl;
        sensor.raw_reading = 0;
    }
    sensor.reading_converted = convert_sensor_reading(
        sensor.sensor_units_1,
        sensor.sensor_linearization,
        sensor.sensor_M,
        sensor.sensor_Mtol,
        sensor.sensor_B,
        sensor.sensor_Bacc,
        sensor.sensor_Acc,
        sensor.R_exp,
        sensor.B_exp,
        sensor.analog_characteristics,
        sensor.raw_reading
    );

    //update the value on the Dbus interface
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. getSensorReading attempting to update dbus interface with updated sensor value" << std::endl;
    if (sensor.sensorIface != nullptr) {
        sensor.sensorIface->value(sensor.reading_converted);
    } else {
        // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. getSensorReading sensor interface does not exist, skipping" << std::endl;
        ;
    }
}

void VITA4611Device::getAllSensorReadings() {
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. getAllSensorReadings enter" << std::endl;
    for (SensorRecord& sensor : fruinfo.sensors) {
        getSensorReading(sensor, 0);
    }
    
//    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Finished getAllSensorReadings" << std::endl;
}

std::tuple<uint8_t, uint8_t> VITA4611Device::reserveSDRRepo() {
    std::optional<ipmb_response_return_type_t> ipmb_response_sdr_reserve;
    std::vector<uint8_t> payload;
    uint8_t ipmi_response_cc;
    ipmb_response_sdr_reserve = sendIPMBRequest(
        channel,
        ipmi_address,
        NETFN_S_E,
        0,
        IPMITOOL_CMD_RESERVE_SDR_REPO,
        payload,
        &ipmi_response_cc,
        false
    );
    ipmb_response_return_type_t resp = ipmb_response_sdr_reserve.value();
    std::vector<uint8_t> data = std::get<5>(resp);

    std::tuple<uint8_t, uint8_t> res_id = std::make_tuple(data[0], data[1]);
    
    dbgFile << "\n";
    dbgFile.flush();

    return res_id;
}

std::vector<uint8_t> VITA4611Device::getSDRHeader(std::tuple<uint8_t, uint8_t> res_id, uint8_t sensor_number) {
    std::optional<ipmb_response_return_type_t> ipmb_response_sdr_header;
    std::vector<uint8_t> payload = {std::get<0>(res_id), std::get<1>(res_id), sensor_number, 0, 0, 0x05};
    uint8_t ipmi_response_cc;
    ipmb_response_sdr_header = sendIPMBRequest(
        channel,
        ipmi_address,
        NETFN_S_E,
        0,
        IPMITOOL_CMD_GET_SDR,
        payload,
        &ipmi_response_cc,
        false
    );
    ipmb_response_return_type_t resp = ipmb_response_sdr_header.value();
    std::vector<uint8_t> data = std::get<5>(resp);
    return data;
}

std::vector<uint8_t> VITA4611Device::getSDRVals(std::tuple<uint8_t, uint8_t> res_id, uint8_t sensor_number, int length) {
    std::vector<uint8_t> response_cumulative;

    uint8_t section = 0x16;
    uint8_t total = 0;
    uint8_t sectiontotal = 0x05;

    do {
        if ((total+section) > length) {
            section = length - total;
        }

        std::optional<ipmb_response_return_type_t> ipmb_response_sdr_vals;
        std::vector<uint8_t> payload = {std::get<0>(res_id), std::get<1>(res_id), sensor_number, 0, sectiontotal, section};
        uint8_t ipmi_response_cc;
        ipmb_response_sdr_vals = sendIPMBRequest(
            channel,
            ipmi_address,
            NETFN_S_E,
            0,
            IPMITOOL_CMD_GET_SDR,
            payload,
            &ipmi_response_cc,
            false
        );
        ipmb_response_return_type_t resp = ipmb_response_sdr_vals.value();
        std::vector<uint8_t> data = std::get<5>(resp);
        if (data.size() != 0) {
            int startIndex = 2; // Get elements after index 2 (i.e., from index 3 onwards)
            std::vector<uint8_t> dataparsed;
            if (startIndex < (int)data.size()) {
                for (size_t i = startIndex; i < data.size(); i++) {
                    dataparsed.push_back(data[i]);
                }
            }
            response_cumulative.insert(response_cumulative.end(), dataparsed.begin(), dataparsed.end());
        }
        total += section;
        sectiontotal += section;
        
    } while (section == 0x16);
    
    // dbgFile << "getSDRvals exit" << std::endl;
    return response_cumulative;
}

bool VITA4611Device::isSensorDiscovered(uint8_t sensorNum) {
    for (SensorRecord& s : fruinfo.sensors) {
        if (s.sensorNumber==sensorNum){
            return true;
        }
    }
    return false;
}

SensorRecord* VITA4611Device::getSensorRecord(uint8_t sensorNum) {
    for (SensorRecord& s : fruinfo.sensors) {
        if (s.sensorNumber == sensorNum) {
            return &s;
        }
    }
    return nullptr;
}

double VITA4611Device::convert_sensor_reading(uint8_t units_def, uint8_t linearization, uint8_t M, uint8_t Mtol, uint8_t B, uint8_t Bacc, uint8_t Acc, 
    uint8_t R_exp, uint8_t B_exp, uint8_t analog_characteristics, uint8_t reading) {

    if ((analog_characteristics & 0x1) == 0) {;}
    if ((Acc & 0x1) == 0) {;}

    uint8_t analog_data_format = units_def >> 6;

    double x;
    if (analog_data_format == 0x00) {  // Unsigned
        x = static_cast<double>(reading);
    } else if (analog_data_format == 0x01) {  // 1's complement signed
        if (reading & 0x80) {
            x = -static_cast<double>((~reading) & 0x7F);  // Negative: invert lower 7 bits
        } else {
            x = static_cast<double>(reading);
        }
    } else if (analog_data_format == 0x02) {  // 2's complement signed
        x = static_cast<double>(static_cast<int8_t>(reading));
    } else {  // 0x03: Non-analog; no conversion
        dbgFile << "Non-analog format; returning raw" << std::endl;
        return static_cast<double>(reading);  // Or throw error
    }

    uint16_t M_unsigned = ((Mtol >> 6) << 8) | M;
    int16_t M_signed = (M_unsigned & 0x0200) ? (M_unsigned | 0xFC00) : M_unsigned;
    uint16_t B_unsigned = ((Bacc >> 6) << 8) | B;
    int16_t B_signed = (B_unsigned & 0x0200) ? (B_unsigned | 0xFC00) : B_unsigned;

    int8_t B_exp_signed = (B_exp & 0x08) ? (B_exp | 0xF0) : B_exp;
    int8_t R_exp_signed = (R_exp & 0x08) ? (R_exp | 0xF0) : R_exp;

    double calc_val = (static_cast<double>(M_signed) * x) + (static_cast<double>(B_signed) * std::pow(10.0, B_exp_signed));
    double result = calc_val * std::pow(10.0, R_exp_signed);
    
    double linearization_calc;
    if (linearization >= 0x70) {
        // dbgFile << "Non-linear sensor; static conversion inaccurate. Using linear fallback." << std::endl;
        linearization_calc = result;
    } else {
        switch (linearization) {
            case 0x00: linearization_calc = result; break;
            case 0x01: linearization_calc = (result > 0) ? std::log(result) : result; break;  // ln; skip if <=0
            case 0x02: linearization_calc = (result > 0) ? std::log10(result) : result; break;
            case 0x03: linearization_calc = (result > 0) ? std::log2(result) : result; break;
            case 0x04: linearization_calc = std::exp(result); break;
            case 0x05: linearization_calc = std::pow(10.0, result); break;
            case 0x06: linearization_calc = std::pow(2.0, result); break;
            case 0x07: linearization_calc = (result != 0) ? 1.0 / result : result; break;
            case 0x08: linearization_calc = std::pow(result, 2.0); break;
            case 0x09: linearization_calc = std::pow(result, 3.0); break;
            case 0x0A: linearization_calc = (result >= 0) ? std::sqrt(result) : result; break;
            case 0x0B: linearization_calc = (result >= 0) ? std::cbrt(result) : result; break;
            default:   linearization_calc = result; break;  // Reserved; treat as linear
        }
    }

    // dbgFile << "result of reading conversion: " << linearization_calc << std::endl;
    return linearization_calc;
}

void VITA4611Device::handleFullSdr(std::vector<uint8_t> sdrheader, std::vector<uint8_t> sdrvals) {
    uint8_t sensorNumber_raw = sdrvals[2];
    uint8_t sensorUnits_1 = sdrvals[15];
    uint8_t sensorUnits = sdrvals[16];

    uint8_t linearization = sdrvals[18];
    uint8_t M = sdrvals[19];
    uint8_t Mtol = sdrvals[20];
    uint8_t B = sdrvals[21];
    uint8_t Bacc = sdrvals[22];
    uint8_t Acc = sdrvals[23];
    uint8_t R_exp = sdrvals[24] >> 4;
    uint8_t B_exp = sdrvals[24] & 0x0f;
    uint8_t analog_characteristics = sdrvals[25];
    uint8_t raw_reading = sdrvals[26];

    double calc_reading = convert_sensor_reading(sensorUnits_1, linearization, M, Mtol,
                            B, Bacc, Acc, R_exp, B_exp,
                            analog_characteristics, raw_reading);

    // dbgFile << "[" << currentTimestamp() << "] SDR Units: " << sensorUnitTypeCodes[(int)sensorUnits] << std::endl; 
    uint8_t idLength = sdrvals[42] & 0x1F;
    // dbgFile << "ID length is " << static_cast<int>(idLength) << std::endl;
    std::vector<uint8_t> sdrvals_name(sdrvals.begin() + 43, sdrvals.begin() + 43 + static_cast<size_t>(idLength));
    std::string SDR_Name_resp(sdrvals_name.begin(), sdrvals_name.end());
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID SDR sensor name " << SDR_Name_resp << std::endl;
    SensorRecord rec;
    // dbgFile << "Attempting to add a new sensor at number " << (int)sensorNumber_raw << std::endl;
    rec.sensorNumber = sensorNumber_raw;
    rec.sdr_header_raw = sdrheader;
    rec.sdr_info_raw = sdrvals;
    rec.SDR_Name = SDR_Name_resp;
    rec.SDR_Units = sensorUnitTypeCodes[(int)sensorUnits];
    rec.sensor_caps = sdrvals[6];
    rec.sensor_type = sdrvals[7];
    rec.sensor_units_1 = sensorUnits_1;
    rec.sensor_units_3 = sdrvals[17];
    rec.sensor_linearization = linearization;
    rec.sensor_M = M;
    rec.sensor_Mtol = Mtol;
    rec.sensor_B = B;
    rec.sensor_Bacc = Bacc;
    rec.sensor_Acc = Acc;
    rec.R_exp = R_exp;
    rec.B_exp = B_exp;
    rec.analog_characteristics = analog_characteristics;
    rec.normal_maximum = sdrvals[27];
    rec.normal_minimum = sdrvals[28];
    rec.sensor_maximum = sdrvals[29];
    rec.sensor_minimum = sdrvals[30];
    rec.upper_nonrec_threshold = sdrvals[31];
    rec.lower_nonrec_threshold = sdrvals[34];
    rec.upper_critical_threshold = sdrvals[32];
    rec.lower_critical_threshold = sdrvals[35];
    rec.upper_noncritical_threshold = sdrvals[33];
    rec.lower_noncritical_threshold = sdrvals[36];
    rec.converted_upper_nonrec_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.upper_nonrec_threshold);
    rec.converted_lower_nonrec_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.lower_nonrec_threshold);
    rec.converted_upper_critical_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.upper_critical_threshold);
    rec.converted_lower_critical_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.lower_critical_threshold);
    rec.converted_upper_noncritical_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.upper_noncritical_threshold);
    rec.converted_lower_noncritical_threshold = convert_sensor_reading(sensorUnits_1,linearization,M,Mtol,B,Bacc,Acc,R_exp,B_exp,analog_characteristics,rec.lower_noncritical_threshold);
    rec.raw_reading = raw_reading;
    rec.reading_converted = calc_reading;
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID SDR sensor name " << SDR_Name_resp << ", raw reading: " << static_cast<int>(rec.raw_reading) << " and threshold upper critical " << static_cast<int>(rec.upper_critical_threshold) << std::endl;
    // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID converted upper nonrec threshold: " << rec.converted_upper_nonrec_threshold << " and converted reading: " << calc_reading << std::endl;
    // dbgFile << "\t reading " << static_cast<int>(rec.raw_reading) << std::endl;
    if (!isSensorDiscovered(sensorNumber_raw)) {fruinfo.sensors.push_back(std::move(rec));}
    else {
        SensorRecord* existingrec = getSensorRecord(sensorNumber_raw);
        if (existingrec) {
            *existingrec = std::move(rec);
        }
    }
}

void VITA4611Device::handleCompactSdr(std::vector<uint8_t> sdrheader, std::vector<uint8_t> sdrvals) {
    //TODO: COMPLETION REQUIRED
    SensorRecord rec;
    uint8_t sensorNumber_raw = sdrvals[2];
    uint8_t sensorUnits = sdrvals[16];
    // dbgFile << "[" << currentTimestamp() << "] SDR Units: " << sensorUnitTypeCodes[(int)sensorUnits] << std::endl; 
    uint8_t idLength = sdrvals[26] & 0x1F;
    // dbgFile << "ID length is " << static_cast<int>(idLength) << std::endl;
    // std::vector<uint8_t> sdrvals_name(sdrvals.begin()+27, sdrvals.end());
    std::vector<uint8_t> sdrvals_name(sdrvals.begin() + 27, sdrvals.begin() + 27 + static_cast<size_t>(idLength));
    std::string SDR_Name_resp(sdrvals_name.begin(), sdrvals_name.end());
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID SDR sensor name " << SDR_Name_resp << std::endl;
    if (isSensorDiscovered(sensorNumber_raw)) {
        SensorRecord* existingRec = getSensorRecord(sensorNumber_raw);
        if (existingRec) {
            dbgFile << "Existing record at " << (int)sensorNumber_raw << std::endl;
            existingRec->sdr_header_raw = sdrheader;
            existingRec->sdr_info_raw = sdrvals;
            existingRec->SDR_Name = SDR_Name_resp;
            existingRec->SDR_Units = sensorUnitTypeCodes[(int)sensorUnits];
        }
    } else {
        // dbgFile << "Attempting to add a new sensor" << std::endl;
        dbgFile << "Attempting to add a new sensor at number " << (int)sensorNumber_raw << std::endl;
        rec.sensorNumber = sensorNumber_raw;
        rec.sdr_header_raw = sdrheader;
        rec.sdr_info_raw = sdrvals;
        rec.SDR_Name = SDR_Name_resp;
        rec.SDR_Units = sensorUnitTypeCodes[(int)sensorUnits];
        fruinfo.sensors.push_back(std::move(rec));
    }
}

void VITA4611Device::getAllSDRs() {
    // dbgFile << "getAllSDRs enter" << std::endl;
    std::tuple<uint8_t, uint8_t> res_id = reserveSDRRepo();
    int retryCount;
    for (uint8_t i = 0; i < device_record_total; i++) {
        // dbgFile << "Attempting to add another SDR at sensor number " << (int)i << std::endl;
        SensorRecord rec;
        try {
            std::vector<uint8_t> sdrheader = getSDRHeader(res_id, i);

            if (sdrheader.size() != 7) {
                dbgFile << "Invalid SDR header for record " << (int)i << ": size=" << sdrheader.size() << std::endl;
                continue;
            }
            uint8_t recordType = sdrheader[5];
            std::vector<uint8_t> sdrvals = getSDRVals(res_id, i, (uint8_t)sdrheader[6]);
            
            try {
                if (recordType == 1) {
                    handleFullSdr(sdrheader, sdrvals);

                } else if (recordType == 2) {
                    handleCompactSdr(sdrheader, sdrvals);

                }
    
            } catch (const std::runtime_error& e) {
                dbgFile << "Caught an exception: " << e.what() << std::endl;
                continue;
            } catch (...) {
                dbgFile << "Caught an unknown exception." << std::endl;
                continue;
            }
        } catch (...) {
            dbgFile << "Failure, retrycount: " << retryCount << std::endl;
            if (retryCount >= 3) {
                return;
            }
            retryCount++;
            continue;
        }
    }   
    
    // dbgFile << "getAllSDRs exit" << std::endl;
}

uint8_t VITA4611Device::get_dynamic_sensors() {
    return dynamic_sensors;
}

void VITA4611Device::checkSDRInfo() {
    
    std::optional<ipmb_response_return_type_t> ipmb_response_sdr_info;
    uint8_t ipmi_response_cc;
    std::vector<uint8_t> emptypayload;
    ipmb_response_sdr_info = 
    sendIPMBRequest(
        0,
        ipmi_address,
        NETFN_S_E,
        0,
        IPMITOOL_CMD_GET_SDR_INFO,
        emptypayload,
        &ipmi_response_cc,
        false);
    if (ipmb_response_sdr_info.has_value()) {
        ipmb_response_return_type_t resp_sdr_info;
        resp_sdr_info = ipmb_response_sdr_info.value();
        auto resp_payload_sdr_info = std::get<5>(resp_sdr_info);
        
        //since dynamic sensor is enabled, the length of resp_payload_sdr_info should be 6
        std::vector<uint8_t> sensorPopulationChangeIndicator(resp_payload_sdr_info.begin()+2, resp_payload_sdr_info.end());

        //if they are different, then that means that something has changed. therefore, retrieve all SDRs again from the FRU
        if (sensorPopulationChangeIndicator != sensorPopulationChangeIndicatorCache) {
            getAllSDRs();
        }

        sensorPopulationChangeIndicatorCache = sensorPopulationChangeIndicator;
    }
}

/*
* discoverFRUs
* Discover all the sensors implemented by each of the FRUs
* implemented by the device
*/
void
VITA4611Device::discoverFRUs()
{
    /* we already know all the FRUs. This is in VSO caps
    *
    */
   uint8_t i;
   if (vsocaps->MaxFRUDeviceID == 0) {
    enumerateFRUSensors(0, this->ipmi_address);
   } else {
    for (i = 0; i < vsocaps->MaxFRUDeviceID; i++) {
        enumerateFRUSensors(i, this->ipmi_address);
    }
   }
   dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Finished discoverFRUs" << std::endl;
}



void
VITA4611Device::queryVsoCapabilities()
{
    std::optional<ipmb_response_return_type_t> ipmb_response;
    uint8_t ipmi_response_cc;
    dbgFile << "LCR:IPMID::ENTRY " << __FUNCTION__ << " " << std::endl;

    std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier

    ipmb_response = sendIPMBRequest(channel, 0x40, NETFN_VITA4611, 0, VITA4611_GETVSOCAPS_CMD, payload, &ipmi_response_cc, false);
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp;
        resp = ipmb_response.value();

        dbgFile << "\n";
        isVITA4611Device = true;

        vsocaps = new VSOCaps;
        *vsocaps = *((struct VSOCaps *) std::get<5>(resp).data());

        //if IPMC is indeed capable, then send command to activate IPMB b.
        if ((vsocaps->IPMBCapabilities & 0x01) == 1) {
            dbgFile << "LCR:IPMID. Device at " << hw_address << " is capable of IPMB b" << std::endl;
            std::vector<uint8_t> payload = {0x03, 0xFF, 0x01}; // VSO Group Extension Identifier
            ipmb_response = 
                sendIPMBRequest(
                    0,
                    ipmi_address,
                    NETFN_VITA4611,
                    0,
                    VITA4611_SET_IPMB_STATE_CMD,
                    payload,
                    &ipmi_response_cc, 
                    true);
        }
    }
    else {
        dbgFile << "LCR:IPMID. Device is not a vita 46.11 device" << std::endl;
        isVITA4611Device = false;
    }
}

VITA4611Device::~VITA4611Device()
{
    if (vsocaps) {
        delete vsocaps;
    }
}

VITA4611Device::VITA4611Device(uint8_t channel, uint8_t ipmi_address)
{
    this->channel = channel;
    this->ipmi_address = ipmi_address;
    this->hw_address = ipmi_address >> 1;
    this->site_number = hw_address - 0x40;
    this->site_type = 0x00;
    
    /*
    * Query device id 
    */
    queryDeviceId(ipmi_address);
    dbgFile <<"LCR:IPMID. Device at " << static_cast<int>(ipmi_address) << " being added and queried." << std::endl;
    if (validDeviceID == true) {
        dbgFile <<"LCR:IPMID. Device implements device id cmd. Querying VSO Capabilities" << std::endl;
        queryVsoCapabilities();

        if (isVITA4611Device == true) {
            /* discover and build the FRU map for this device */
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Device is a vita 46.11 device" << std::endl;
            discoverFRUs();
        }
    }
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Finished initializing VITA4611Device" << std::endl;
}

/*
* Implementation of VITA4611_ChassisManager
* This creates a thread that services requests
* from ipmitool or over RMCP+ or other channels.
*/

VITA4611_ChassisManager::VITA4611_ChassisManager(): bus(sdbusplus::bus::new_default())
{
    dbgFile << "[" << currentTimestamp() << "] "  << "LCR:IPMI::Constructor" << std::endl;

    this->deviceId = DEVICE_ID;
    this->deviceRevision = DEVICE_REVISION;
    this->firmwareRevision1 = FIRMWARE_REVISION_1;
    this->firmwareRevision2 = FIRMWARE_REVISION_2;
    this->IPMIVersion = IPMI_VERSION;
    this->additionalDeviceSupport = ADDITIONAL_DEVICE_SUPPORT;
    this->manufacturerID1 = MANUFACTURER_ID_1;
    this->manufacturerID2 = MANUFACTURER_ID_2;
    this->productID1 = PRODUCT_ID_1;
    this->productID2 = PRODUCT_ID_2;

    this->chassisAddressTable.record_type_id = 0xC0;
    this->chassisAddressTable.version = 0x82;
    this->chassisAddressTable.record_length = 0x0;
    this->chassisAddressTable.record_checksum = 0x0;
    this->chassisAddressTable.header_checksum = 0x0;
    this->chassisAddressTable.manufacturer_id = {0x00, 0x81, 0xAC};
    this->chassisAddressTable.vita_record_id = 0x10;
    this->chassisAddressTable.record_format_version = 0x0;
    this->chassisAddressTable.chassis_id_type = 0xC0;

    //TODO figure out what this means
    //TODO figure out what needs to go into this field
    //this->chassisAddressTable.chassis_id_field;

    this->chassisAddressTable.chassis_add_table_entries_count = 0x0;

    this->address_count=1;
    this->site_type=0;
    this->ipmb_address=0x40;
    this->site_number=this->ipmb_address - 0x40;
    this->max_unavailable_time = 0x0a;
    this->address_type = 0x80;
    //add the details to the IP address. 192.168.0.104
    this->ip_address.push_back(0xc0);
    this->ip_address.push_back(0xa8);
    this->ip_address.push_back(0x00);
    this->ip_address.push_back(0x68);
    this->ip_address.push_back(0x02);
    this->ip_address.push_back(0x6f);

    this->m_ipmb_a_enable = 1;
    this->m_ipmb_b_enable = 1;


    /*
    * enumerate i2c devices and join them to the map
    * If they are also vita 46.11 devices then they also
    * get initialized
    */
    enumerateI2CDevices();

    /*
    * Once we have enumerated the devices,
    * see if they are vita 46.11 compliant devices
    * To do this, send get device id and if that works
    * then send a get vso capabilities command and cache the
    * information.
    */
    worker = std::thread(&VITA4611_ChassisManager::processLoop, this);

    //run this to expose the sensors discovered in the sub-frus
    exposeFruSensors();
}

VITA4611_ChassisManager::~VITA4611_ChassisManager()
{
    if (worker.joinable())
    {
        sendShutdown();
        worker.join();
    }

    int fd = bus.get_fd();
    shutdown(fd, SHUT_RD);
}

bool
VITA4611_ChassisManager::verify_ipmi_address_is_valid(const uint8_t ipmi_address)
{
        /* we have a list of all the i2c devices on all i2c buses
    * in our device map
    * send get device id to each of them and then get vso capabilities
    * and store these.
    * When ipmitool command is sent for these commands, we will return
    * these cached values. For everything else, we will send them out on
    * the i2c bus
    */
   for (const auto& pair: i2cdeviceMap) {
        const std::vector<I2CDeviceInfo>& vec = pair.second;

        for (const auto &item: vec) {
            if (item.devicei2caddress == (ipmi_address >> 1)) {
                return true;
            }
        }
    }

    return false;
}

void
VITA4611_ChassisManager::enumerateI2CDevices()
{
    int bus;
    int file;

    int n = 1;
    /*
    * Only enumerate on bus 0 which is IPMB-A
    * on the LCR board.
    * 
    * We will enable ipmb-b later if we decide that
    * there is at least one device that is a tier3 device
    */
    for (bus = 0; bus < 1; bus++) {
        
        std::string device = "/dev/i2c-" + std::to_string(bus);

        file = open(device.c_str(), O_RDWR);
        if (file < 0) {
            continue;
        }

        /* bus is present now query devices on the bus */
        /* walk through 2 buses connected to the PS of zc702 and discover
        *  devices. At this point we do not care yet whether these are
        *  IPMI or VITA 46.11 devices
        */

        for (uint8_t addr = 0x3; addr <= 0x77; addr++) {

            if (ioctl(file, I2C_SLAVE, addr) ) {
                continue;
            }

            /* There is a i2c device at addr.
            *  We are the ShMC and our address is 0x20
            *  so ignore that
            */
           // THE CHASSIS MANAGER SHOULD BE FRU DEVICE 0
           if ( addr == CHMC_I2C_ADDRESS ) {
                dbgFile << "[" << currentTimestamp() << "] "  << "LCR:IPMI::enumerate I2c Devices. Chassis Manager IP address discovered" << std::endl;
                if (write(file, nullptr, 0) >= 0) {
                        /*
                        * add the device
                        */
                    
                    i2cdeviceMap[bus].push_back({bus, addr, bus, 0x20});
                    deviceMap[0] = new VITA4611Device(0, addr << 1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    deviceMap[0]->device_id = 0;
                    CAT_entry entry;
                    entry.hw_address = deviceMap[0]->hw_address;

                    //0 because: As noted in Table 10.3.1-2, a Chassis Address Table Entry which represents a Front Loading VPX Module 
                    //or an RTM module uses a 0 in the Chassis Address Table Site Number field.
                    entry.site_num = 0;
                    entry.site_type = deviceMap[0]->site_type;
                    chassisAddressTable.entries.push_back(entry);
                    chassisAddressTable.chassis_add_table_entries_count += 0x01;
                }
           } else {
                if (write(file, nullptr, 0) >= 0) {
                        /*
                        * add the device
                        */
                    
                    i2cdeviceMap[bus].push_back({bus, addr, bus, 0x20});
                    deviceMap[n] = new VITA4611Device(0, addr << 1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    deviceMap[n]->device_id = 0;
                    CAT_entry entry;
                    entry.hw_address = deviceMap[n]->hw_address;

                    //0 because: As noted in Table 10.3.1-2, a Chassis Address Table Entry which represents a Front Loading VPX Module 
                    //or an RTM module uses a 0 in the Chassis Address Table Site Number field.
                    entry.site_num = 0;
                    entry.site_type = deviceMap[n]->site_type;
                    chassisAddressTable.entries.push_back(entry);
                    chassisAddressTable.chassis_add_table_entries_count += 0x01;
                    n += 1;
                }
           }
        }

        close(file);
    }
}


std::future<std::vector<uint8_t>> VITA4611_ChassisManager::submit(VITA4611Command req)
{
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMI. Command submitted" << std::endl;
    std::lock_guard<std::mutex> lock(queueMutex);
    auto fut = req.promise.get_future();
    requestQueue.push(std::move(req));
    queueCond.notify_one();
    dbgFile << "LCR:IPMI. Command completed" << std::endl;
    return fut;
}

void VITA4611_ChassisManager::sendShutdown()
{
    VITA4611Command shutdownReq;
    dbgFile << "[" << currentTimestamp() << "] " << "Sending chassis manager thread command to shutdown" << std::endl;
    shutdownReq.type = VITA4611CMCommandType::Shutdown;
    std::lock_guard<std::mutex> lock(queueMutex);
    requestQueue.push(std::move(shutdownReq));
    queueCond.notify_one();
}

void VITA4611_ChassisManager::processLoop()
{
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMI. worker thread started" << std::endl;
    while (true)
    {
        VITA4611Command req;
        {
            dbgFile << "LCR:IPMI. Handling queue" << std::endl;
            std::unique_lock<std::mutex> lock(queueMutex);
            dbgFile << "LCR:IPMI. Lock until queue is empty now, I think" << std::endl;
            queueCond.wait(lock, [this] {
                return !requestQueue.empty();
            });
            dbgFile << "LCR:IPMI. Pop queue" << std::endl;
            req = std::move(requestQueue.front());
            requestQueue.pop();
        }
        dbgFile << "LCR:IPMI. handling if shutdown" << std::endl;
        if (req.type == VITA4611CMCommandType::Shutdown)
        {
            dbgFile << "LCR:IPMI. Thread received shutdown command" << std::endl;
            break;
        }

        dbgFile << "LCR:IPMI. Thread received a command to process" << std::endl;
        std::vector<uint8_t> result = handleRequest(req);
        req.promise.set_value(result);
    }
    dbgFile << "LCR:IPMI. worker thread is exiting" << std::endl;
    dbgFile.flush();
}

std::vector<uint8_t> VITA4611_ChassisManager::handleRequest(const VITA4611Command& req)
{
    dbgFile << "[" << currentTimestamp() << "] " << "Handling request for chassis manager" << std::endl;
    (void)req;
    dbgFile << "Done handling request for chassis manager" << std::endl;
    dbgFile.flush();
    return {0x00}; // Dummy success response
}


void VITA4611_ChassisManager::get_device_list(std::vector<uint8_t>& ipmi_address_list)
{
    dbgFile << "LCR:IPMI: Running ChM getdevicelist" << std::endl;
    dbgFile.flush();
    for (const auto& pair: i2cdeviceMap) {
        const std::vector<I2CDeviceInfo>& vec = pair.second;

        for (const auto &item: vec) {
            /* ipmi address is always i2c address * 2
            */
            ipmi_address_list.push_back(item.devicei2caddress << 1);
        }
    }
}

std::vector<uint8_t> VITA4611_ChassisManager::getShMCVSOCaps()
{
    /*
    * We are chassis manager
    * Tier 3
    * These are fixed by the implementation
    */
   const uint8_t IPMCIdentifier = (1 << 4) | 2;
   const uint8_t IPMBCapabilities = 1; /* implement both ipmb-a and ipmb-b */
   const uint8_t VSOStandard = 0; /* vita 4611. */
   const uint8_t VSOStandardRevision = 0;
   const uint8_t MaxFRUDeviceId = 1;
   const uint8_t IPMFRUDeviceId = 0;

   std::vector<uint8_t> responseData = {
       IPMCIdentifier,
       IPMBCapabilities,
       VSOStandard,
       VSOStandardRevision,
       MaxFRUDeviceId,
       IPMFRUDeviceId
   };
//    responseData.insert(responseData.begin(), 0x03);
   return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getAddTable(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Calling get Add table" << std::endl;
    dbgFile.flush();

    std::vector<uint8_t> responseData;
    std::vector<uint8_t> emptyData = {};


    if (reqdata.size() < 8) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] No arguments specified" << std::endl;
        return emptyData;
    }
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Parsing request data" << std::endl;
    uint8_t fru_dev_id = reqdata[4];
    uint8_t addr_key_type = reqdata[5];
    uint8_t addr_key = reqdata[6];
    uint8_t site_type = reqdata[7];

    if (addr_key_type > 0x03 || addr_key_type == 0x02 || chassisAddressTable.entries.empty()) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] No devices present or impossible key type specified" << std::endl;
        return emptyData;
    } else if (addr_key_type == 0x03 && site_type == 0) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] address key is physical address" << std::endl;
        VITA4611Device *Dev = findDeviceByPhysAddress(addr_key);
        responseData = {
            Dev->hw_address,
            Dev->ipmi_address,
            0xFF,
            Dev->device_id,
            Dev->site_number,
            Dev->site_type
        };
    } else {
        VITA4611Device *Dev;
        if (addr_key_type == 0x00) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] address key is hardware address" << std::endl;
            Dev = findDeviceByHwAddress(addr_key);
        } else if (addr_key_type == 0x01) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] address key is ipmb address" << std::endl;
            Dev = findDeviceByAddress(addr_key);
        }

        //TODO: Implement requesting info over the IPMB
        if (fru_dev_id > 0) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] fru address is not 0. ask the ipmc for the info" << std::endl;
            std::optional<ipmb_response_return_type_t> ipmb_response;
            uint8_t ipmi_response_cc;
            std::vector<uint8_t> payload = {0x03, fru_dev_id}; // VSO Group Extension Identifier
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] sending request for fru info" << std::endl;
            ipmb_response = 
                sendIPMBRequest(
                    0,
                    Dev->ipmi_address,
                    NETFN_VITA4611,
                    0,
                    VITA4611_GET_FRU_ADDRESS_INFO_CMD,
                    payload,
                    &ipmi_response_cc, 
                    false);
            if (ipmb_response.has_value()) {
                ipmb_response_return_type_t resp;
                resp = ipmb_response.value();
                auto fru_address_info = std::get<5>(resp);
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] ipmb response for the device id. first byte: " << static_cast<int>(fru_address_info[0]) << ", length of response: "
                    << fru_address_info.size() << std::endl;
                responseData = {
                    fru_address_info[1],
                    fru_address_info[2],
                    fru_address_info[3],
                    fru_address_info[4],
                    fru_address_info[5],
                    fru_address_info[6]
                };
                responseData.insert(responseData.begin(), 0x03);
                return responseData;
            } else {
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] did not receive a response from the ipmc" << std::endl;
            }
        }

        responseData = {
            Dev->hw_address,
            Dev->ipmi_address,
            0xFF,
            Dev->device_id,
            Dev->site_number,
            Dev->site_type
        };
    }
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}



std::vector<uint8_t> VITA4611_ChassisManager::getChassisManagerIpAddresses(uint8_t addr_num) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Chassis Manager IP Addresses command running now" << std::endl;
    std::vector<uint8_t> emptyData = {};
    if (addr_num > 0) {
        return emptyData;
    }

    std::vector<uint8_t> responseData = {
        address_count,
        site_type,
        site_number,
        max_unavailable_time,
        address_type
    };

    for (uint8_t byte : ip_address) {
        responseData.push_back(byte);
    };
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
};

std::vector<uint8_t> VITA4611_ChassisManager::getChassisIdentifier() {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Chassis Identifier command running now" << std::endl;

    std::vector<uint8_t> responseData;

    responseData.push_back(chassisAddressTable.chassis_id_type);
    for (uint8_t byte : chassisAddressTable.chassis_id_field) {
        responseData.push_back(byte);
    }
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
};

std::vector<uint8_t> VITA4611_ChassisManager::setChassisIdentifier(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Chassis Identifier command running now" << std::endl;

    uint8_t chIdentifier = reqdata[4];

    std::vector<uint8_t> chIdField;
    chIdField = std::vector<uint8_t>(reqdata.begin() + 5, reqdata.end());
    
    std::vector<uint8_t> emptydata;
    if (chIdField.size() > 20) {
        return emptydata;
    }

    chassisAddressTable.chassis_id_type = chIdentifier;
    int c = 0;
    for (uint8_t byte : chIdField) {
        chassisAddressTable.chassis_id_field[c] = byte;
        c += 1;    
    } 

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
};

std::vector<uint8_t> VITA4611_ChassisManager::fruControl(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] fru control command running now" << std::endl;

    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 5) {
        return emptydata;
    }

    uint8_t fru_dev_id = reqdata[4];
    uint8_t fru_control_byte = reqdata[5];
    VITA4611Device *pdev = deviceMap[fru_dev_id];

    uint8_t ch = 0;
    std::optional<ipmb_response_return_type_t> ipmb_response;
    if (fru_control_byte == 0x00) {
        uint8_t cmd = 0x02;
        uint8_t netFn = 0x06;
        uint8_t lun = 0x00;
        std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
        uint8_t ipmi_response_cc;
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] sending cold reset" << std::endl;
        
        //VITA4611Device *p = deviceMap[0];
        if (pdev) {
            /* payload should start with the VSO identifier
            *  followed by the user payload
            */
            ipmb_response = sendIPMBRequest(
                ch,
                0x8a, //TODO make this a variableSSS
                netFn,
                lun,
                cmd,
                payload,
                &ipmi_response_cc,
                false);
        }
    } else if (fru_control_byte == 0x01) {
        uint8_t cmd = 0x03;
        uint8_t netFn = 0x06;
        uint8_t lun = 0x00;
        std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
        uint8_t ipmi_response_cc;
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] sending warm reset" << std::endl;
        
        //VITA4611Device *p = deviceMap[0];
        if (pdev) {
            /* payload should start with the VSO identifier
            *  followed by the user payload
            */
            ipmb_response = sendIPMBRequest(
                ch,
                0x8a,
                netFn,
                lun,
                cmd,
                payload,
                &ipmi_response_cc,
                false);
        }
    } else if (fru_control_byte == 0x02) {
        // uint8_t cmd = 0x02;
        // uint8_t netFn = 0x06;
        // uint8_t lun = 0x00;
        // std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
        // uint8_t ipmi_response_cc;
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] !!NOT IMPLEMENTED!! sending graceful reboot" << std::endl;
        //TODO figure out graceful reboot
        //VITA4611Device *p = deviceMap[0];
        // if (pdev) {
        //     /* payload should start with the VSO identifier
        //     *  followed by the user payload
        //     */
        //     ipmb_response = sendIPMBRequest(
        //         ch,
        //         netFn,
        //         lun,
        //         cmd,
        //         payload,
        //         &ipmi_response_cc);
        // }
    } else if (fru_control_byte == 0x03) {
        // uint8_t cmd = 0x02;
        // uint8_t netFn = 0x06;
        // uint8_t lun = 0x00;
        // std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
        // uint8_t ipmi_response_cc;
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] !!NOT IMPLEMENTED!! sending diagnostic interrupt" << std::endl;
        //TODO figure out diagnostic interrupt
        //VITA4611Device *p = deviceMap[0];
        // if (pdev) {
        //     /* payload should start with the VSO identifier
        //     *  followed by the user payload
        //     */
        //     ipmb_response = sendIPMBRequest(
        //         ch,
        //         netFn,
        //         lun,
        //         cmd,
        //         payload,
        //         &ipmi_response_cc);
        // }
    } else {
        return emptydata;
    }

    if (ipmb_response.has_value()) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Response received" << std::endl;
    }

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
};

std::vector<uint8_t> VITA4611_ChassisManager::setIpmbState(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set IPMB state command running now" << std::endl;

    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 6) {
        return emptydata;
    }
    //see Table 10.1.3.7-1 in the Vita 46.11 specification
    uint8_t ipmb_a_state = reqdata[4];
    uint8_t ipmb_b_state = reqdata[5];

    if (ipmb_a_state == 0xFF) {
        ;
    } else {
        //only care about the first bit of this request for the enable/disable
        if (ipmb_a_state % 2 == 0) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] turning off ipmb a" << std::endl;
            m_ipmb_a_enable = 0;
        } else {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] turning on ipmb a" << std::endl;
            m_ipmb_a_enable = 1;
        };
        //ID is the state right shifted by 1
        m_ipmb_a_Link_ID = ipmb_a_state >> 1;
    }

    if (ipmb_b_state == 0xFF) {
        ;
    } else {
        if (ipmb_b_state % 2 == 0) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] turning off ipmb b" << std::endl;
            m_ipmb_b_enable = 0;
        } else {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] turning on ipmb b" << std::endl;
            m_ipmb_b_enable = 1;
        };
        //ID is the state right shifted by 1
        m_ipmb_b_Link_ID = ipmb_b_state >> 1;
    }

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
};

std::vector<uint8_t> VITA4611_ChassisManager::setFruStatePolicyBits(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set FRU State Policy Bits command running now" << std::endl;

    std::vector<uint8_t> emptydata;

    if (reqdata.size() < 7) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }

    uint8_t fru_dev_id = reqdata[4];
    uint8_t fru_mask_bits = reqdata[5];
    uint8_t fru_policy_bits = reqdata[6];

    if (deviceMap.empty()){
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] no devices present" << std::endl;
        return emptydata;
    }

    VITA4611Device *dev = deviceMap[(int)fru_dev_id];

    uint8_t fru_activation_policy_l = fru_mask_bits & fru_policy_bits;
    dev->fru_activation_policy = fru_activation_policy_l;

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getFruStatePolicyBits(uint8_t devId) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set FRU State Policy Bits command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;
    if (deviceMap.empty()){
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] no devices present" << std::endl;
        return emptydata;
    }

    VITA4611Device *dev = deviceMap[(int)devId];

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    responseData.push_back(dev->fru_activation_policy);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::setFruActivation(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set FRU State Policy Bits command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;

    if (reqdata.size() < 6) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }

    uint8_t devId = reqdata[4];
    uint8_t FRU_directive = reqdata[5];

    if (deviceMap.empty()){
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] no devices present" << std::endl;
        return emptydata;
    }

    VITA4611Device *dev = deviceMap[(int)devId];

    if (FRU_directive == 0) {
        //TODO write code for deactivating FRU
        dev->fru_activation_flag = FRU_directive;
    } else if (FRU_directive == 1) {
        //TODO write code for activating FRU
        dev->fru_activation_flag = FRU_directive;
    } else {
        return emptydata;
    }

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getDeviceLocatorRecordId(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set FRU State Policy Bits command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;

    if (reqdata.size() < 5) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }

    uint8_t devId = reqdata[4];

    if (deviceMap.empty()){
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] no devices present" << std::endl;
        return emptydata;
    }

    VITA4611Device *dev = deviceMap[(int)devId];
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] device found at device ID: " << static_cast<int>(dev->device_id) << std::endl;

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getChassisManagerIpmbAddress() {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Chassis Manager IPMB Address command running now" << std::endl;

    std::vector<uint8_t> responseData;

    responseData.push_back(0x20);
    responseData.push_back(0x0);

    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}


/////////////////////////////////////////////////
// FAN CONTROL COMMANDS BELOW
// THE CHASSIS MANAGER CONTROLS FANS DIRECTLY
// THEREFORE, IT IS NECESSARY FOR IT TO PROVIDE THE FOLLOWING COMMANDS
/////////////////////////////////////////////////

std::vector<uint8_t> VITA4611_ChassisManager::getFanSpeedProperties(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Fan Speed Properties command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 5) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }
    
    //this does not matter, as the fans are always controlled by the chassis manager in this case
    // uint8_t dev_id = reqdata[4];

    std::vector<uint8_t> responseData;

    //this is constant. fans are controlled by pwm between 0 and 255
    responseData.push_back(0);
    responseData.push_back(0xfd);
    responseData.push_back(0x64);
    //local control supported by fan controller software. return byte 10000000
    responseData.push_back(0x80);

    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getFanLevel(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Fan Level command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 5) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }
    
    //this does not matter, as the fans are always controlled by the chassis manager in this case
    // uint8_t dev_id = reqdata[4];

    std::vector<uint8_t> responseData;

    std::string sense_interface = "xyz.openbmc_project.Control.FanPwm";
    std::string property = "Target";
    std::string path = "/xyz/openbmc_project/control/fanpwm/pwm1";

    std::string systemdMgrIface = "org.freedesktop.systemd1.Manager";
    std::string systemdPath = "/org/freedesktop/systemd1";
    std::string systemdService = "org.freedesktop.systemd1";
    std::string phosphorServiceName = "LCR_fan_controller.service";

    uint64_t fanpwm = 0;
    try {
        fanpwm = phosphor::interface::util::getProperty<uint64_t>(bus, path, sense_interface, property);
    } catch (...) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Fan Level failed the dbus call" << std::endl;
    }
    
    uint8_t fanpwmbyte = static_cast<uint8_t>(fanpwm);
    uint8_t policy;
    policy = isServiceActive("LCR_fan_controller.service") ? 1 : 0;

    // byte 1 and byte 2 are the cc and vso identifier respectively
    responseData.push_back(0xff); // byte 3
    responseData.push_back(fanpwmbyte); // byte 4
    responseData.push_back(policy); // byte 5
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] get Fan Level got fan level " << static_cast<int>(fanpwmbyte) << std::endl;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::setFanLevel(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Fan Level command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 7) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }
    
    //dev id unused
    // uint8_t dev_id = reqdata[4];
    
    uint8_t fan_level = reqdata[5];
    uint8_t local_control_enable_state = reqdata[6];

    std::string sense_interface = "xyz.openbmc_project.Control.FanPwm";
    std::string property = "Target";
    //TODO: EXPAND THIS TO ALL OF THE FANS' PWMS
    std::string path = "/xyz/openbmc_project/control/fanpwm/pwm1";
    
    std::string systemdMgrIface = "org.freedesktop.systemd1.Manager";
    std::string systemdPath = "/org/freedesktop/systemd1";
    std::string systemdService = "org.freedesktop.systemd1";
    std::string phosphorServiceName = "LCR_fan_controller.service";

    int fanpwmint = static_cast<int>(fan_level);
    
    if (local_control_enable_state == 0) {
        try {
            // stop the fan-control service
            phosphor::interface::util::callMethodAndRead<sdbusplus::message::object_path>(
                systemdService, systemdPath, systemdMgrIface, "StopUnit",
                phosphorServiceName, "replace");
            phosphor::interface::util::setProperty<uint64_t>(bus, path, sense_interface, property, fanpwmint);
        } catch (...) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Fan Level failed the dbus call" << std::endl;
        }    
    } else {
        try {
            // start the fan-control service
            phosphor::interface::util::callMethodAndRead<sdbusplus::message::object_path>(
                systemdService, systemdPath, systemdMgrIface, "StartUnit",
                phosphorServiceName, "replace");
        } catch (...) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] starting fan control failed" << std::endl;
        }
    }
    

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

/////////////////////////////////////////////////
//END OF FAN CONTROL COMMANDS
/////////////////////////////////////////////////

std::vector<uint8_t> VITA4611_ChassisManager::setFanPolicy(const std::vector<uint8_t>& reqdata) {
    std::string systemdMgrIface = "org.freedesktop.systemd1.Manager";
    std::string systemdPath = "/org/freedesktop/systemd1";
    std::string systemdService = "org.freedesktop.systemd1";
    std::string phosphorServiceName = "LCR_fan_controller.service";

    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Fan Policy command running now" << std::endl;
    
    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 9) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }

    //This should always be 0
    uint8_t tray_site_num = reqdata[4];
    
    uint8_t fan_enable_state = reqdata[5];

    uint8_t fan_policy_timeout = reqdata[6];

    //the following are optional and currently do not do anything
    // uint8_t site_number = reqdata[7];
    // uint8_t site_type = reqdata[8];

    //handle site number
    if (tray_site_num != 0) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] tray site number must be 0" << std::endl;
        return emptydata;
    }

    const uint8_t MAX_TIMEOUT_SECONDS = 240; // 20 minutes
    if (fan_policy_timeout > MAX_TIMEOUT_SECONDS) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] timeout exceeds maximum of 1200 seconds (20 minutes)" << std::endl;
        return emptydata;
    }

    // Cancel any existing timer
    if (fanRestartTimer) {
        fanRestartTimer->cancel();
        fanRestartTimer.reset();
    }

    //handle fan enable and tiemout
    if (fan_enable_state == 0) {
        try {
            // stop the fan-control service
            if (fan_policy_timeout != 0){
                phosphor::interface::util::callMethodAndRead<sdbusplus::message::object_path>(
                    systemdService, systemdPath, systemdMgrIface, "StopUnit",
                    phosphorServiceName, "replace");
            }
            
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID Fan service stopped" << std::endl;
            //handle timeout
            // Only set timer if timeout is non-zero
            if (fan_policy_timeout > 0 && fan_policy_timeout != 0xFF) {
                // Get the event loop
                auto io = getIoContext();
                // Create timer for automatic restart
                fanRestartTimer = std::make_shared<boost::asio::steady_timer>(*io);
                // Convert timeout to seconds
                auto timeoutDuration = std::chrono::seconds(fan_policy_timeout * 5);
                fanRestartTimer->expires_after(timeoutDuration);
                // Set async callback to restart service
                fanRestartTimer->async_wait([this, systemdService, systemdPath, 
                                            systemdMgrIface, phosphorServiceName]
                                            (const boost::system::error_code& ec) {
                    if (!ec) {
                        try {
                            // Restart the fan-control service
                            phosphor::interface::util::callMethodAndRead<sdbusplus::message::object_path>(
                                systemdService, systemdPath, systemdMgrIface, "StartUnit",
                                phosphorServiceName, "replace");
                            
                            dbgFile << "[" << currentTimestamp() << "] " 
                                << "LCR:IPMID Fan service auto-restarted after timeout" 
                                << std::endl;
                        } catch (...) {
                            dbgFile << "[" << currentTimestamp() << "] " 
                                << "LCR:IPMID[ERROR] Failed to auto-restart fan service" 
                                << std::endl;
                        }
                    } else if (ec != boost::asio::error::operation_aborted) {
                        dbgFile << "[" << currentTimestamp() << "] " 
                            << "LCR:IPMID[ERROR] Timer error: " << ec.message() 
                            << std::endl;
                    }
                        // If operation_aborted, timer was cancelled (new command received)
                });

                dbgFile << "[" << currentTimestamp() << "] " 
                       << "LCR:IPMID Fan restart scheduled in " 
                       << (static_cast<int>(fan_policy_timeout) * 5) << " seconds" 
                       << std::endl;
            } else if (fan_policy_timeout != 0xFF) {
                dbgFile << "[" << currentTimestamp() << "] " 
                       << "LCR:IPMID Fan service stopped indefinitely (timeout=0xFF)" 
                       << std::endl;
            } else {
                dbgFile << "[" << currentTimestamp() << "] " 
                       << "LCR:IPMID fan service not stopped since the timeout is <= 0" 
                       << std::endl;
            }
        } catch (...) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Fan Policy, stop Fan control failed the dbus call" << std::endl;
        }    
    } else {
        try {
            // start the fan-control service
            phosphor::interface::util::callMethodAndRead<sdbusplus::message::object_path>(
                systemdService, systemdPath, systemdMgrIface, "StartUnit",
                phosphorServiceName, "replace");
        } catch (...) {
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] set Fan Policy, start Fan control failed the dbus call" << std::endl;
        }
    }

    std::vector<uint8_t> responseData;
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getFanPolicy(const std::vector<uint8_t>& reqdata) {
    std::vector<uint8_t> emptydata;
    if (reqdata.size() < 7) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] not enough arguments" << std::endl;
        return emptydata;
    }

    uint8_t tray_site_number = reqdata[4];

    uint8_t site_number = reqdata[5];

    uint8_t site_type = reqdata[6];

    std::vector<uint8_t> responseData;

    uint8_t policy;
    if (tray_site_number != 0xFF) {
        // Check if LCR_fan_controller.service is active
        policy = isServiceActive("LCR_fan_controller.service") ? 1 : 0;
    } else {
        policy = 0;
    }
    
    uint8_t coverage;
    if (site_number != 0xFF || site_type != 0xFF) {
        coverage = 1; //all sites are covered by the fan
    }

    responseData.push_back(policy);
    responseData.push_back(coverage);
    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::get_chm_fru_info() {
    //TODO: Implement and add prototype to chm class

    std::vector<uint8_t> responseData;

    responseData.push_back(0x20); // hw address
    responseData.push_back(0x40); // ipmb address
    responseData.push_back(0xFF); // reserved
    responseData.push_back(0x00); // FRU Device ID
    responseData.push_back(0x00); // Site Number
    responseData.push_back(0x00); // Site Type
    responseData.push_back(0xFF); // reserved
    responseData.push_back(0xFF); // Address on ch 7. irrelevant

    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::getFruAddressInfo(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Calling get Fru Address Info" << std::endl;
    dbgFile.flush();

    std::vector<uint8_t> emptydata;
    std::vector<uint8_t> responseData;

    if (reqdata.size() < 4 || (reqdata.size() > 4 && reqdata.size() < 8) || reqdata.size() > 8) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] incorrect number of arguments" << std::endl;
        return emptydata;
    }
    if (reqdata.size() == 4) {
        return get_chm_fru_info();
    }

    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Parsing request data" << std::endl;
    uint8_t fru_dev_id = reqdata[4];
    uint8_t addr_key_type = reqdata[5];
    uint8_t addr_key = reqdata[6];
    uint8_t site_type = reqdata[7];

    if (fru_dev_id != 0) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Device ID must be 0, since the chassis manager is the only FRU present in this system." << std::endl;
        return emptydata;
    }

    if (addr_key_type > 0x03 || addr_key_type < 0x03) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] FRU Address Info request with key type that is not 0x03 (Physical Address). Error." << std::endl;
        return emptydata;
    } else if (addr_key_type == 0x03 && site_type == 0) {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] address key is physical address" << std::endl;
        VITA4611Device *Dev = findDeviceByPhysAddress(addr_key);
        responseData = {
            Dev->hw_address,
            Dev->ipmi_address,
            0xFF,
            Dev->device_id,
            Dev->site_number,
            Dev->site_type,
            0xFF,
            0xFF
        };
    } else {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] site type not acceptable" << std::endl;
    }

    responseData.insert(responseData.begin(), 0x03);
    return responseData;
}

std::vector<uint8_t> VITA4611_ChassisManager::handleChMCommand(const std::vector<uint8_t>& reqdata) {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] ChM command detected, handling now" << std::endl;

    //uint8_t ipmi_address = reqdata[0];
    uint8_t netFn = reqdata[1];
    //uint8_t lun = reqdata[2];
    uint8_t cmd = reqdata[3];

    std::vector<uint8_t> emptyData = {};

    if (netFn == NETFN_VITA4611) {
        switch (cmd) {
            case VITA4611_GETVSOCAPS_CMD: 
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get VSO caps detected" << std::endl;
                return getShMCVSOCaps();
            case VITA4611_GET_CHASSIS_ADDRESS_TABLE_INFO_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get chassis address table info detected" << std::endl;
                return getAddTable(reqdata);
            case VITA4611_CHASSIS_MANAGER_IP_ADDRESSES_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get chassis manager ip addresses detected" << std::endl;
                if (reqdata.size() > 4) {
                    return getChassisManagerIpAddresses(reqdata[4]);
                } else {
                    return emptyData;
                }
            case VITA4611_GET_CHASSIS_ID_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get chassis identifier detected" << std::endl;
                return getChassisIdentifier();
            case VITA4611_SET_CHASSIS_ID_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set chassis identifier detected" << std::endl;
                return setChassisIdentifier(reqdata);
            case VITA4611_FRU_CONTROL: 
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] FRU Control detected" << std::endl;
                return fruControl(reqdata);
            case VITA4611_SET_IPMB_STATE_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set IPMB State detected" << std::endl;
                return setIpmbState(reqdata);
            case VITA4611_SET_FRU_STATE_POLICY_BITS_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set FRU State Policy Bits detected" << std::endl;
                return setFruStatePolicyBits(reqdata);
            case VITA4611_GET_FRU_STATE_POLICY_BITS_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get FRU State Policy Bits detected" << std::endl;
                if (reqdata.size() > 4) {
                    return getFruStatePolicyBits(reqdata[4]);
                } else {
                    return emptyData;
                }
            case VITA4611_SET_FRU_ACTIVATION_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set FRU Activation detected" << std::endl;
                return setFruActivation(reqdata);
            case VITA4611_GET_DEVICE_LOCATOR_RECORD_ID_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Device Locator Record ID detected" << std::endl;
                return getDeviceLocatorRecordId(reqdata);

            //OPTIONAL FAN CONTROL COMMANDS
            case VITA4611_GET_FAN_SPEED_PROPERTIES_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Fan Speed Properties detected" << std::endl;
                return getFanSpeedProperties(reqdata);
            case VITA4611_SET_FAN_LEVEL_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set Fan Level detected" << std::endl;
                return setFanLevel(reqdata);
            case VITA4611_GET_FAN_LEVEL_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Fan Level detected" << std::endl;
                return getFanLevel(reqdata);
            //END OF OPTIONAL FAN CONTROL COMMANDS


            case VITA4611_SET_FAN_POLICY_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Set Fan Policy detected" << std::endl;
                return setFanPolicy(reqdata);
            case VITA4611_GET_FAN_POLICY_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Fan Policy detected" << std::endl;
                return getFanPolicy(reqdata);
            case VITA4611_GET_CHASSIS_MANAGER_IPMB_ADDRESS_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Chassis Manager IPMB Address detected" << std::endl;
                return getChassisManagerIpmbAddress();
            case VITA4611_GET_FRU_ADDRESS_INFO_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get FRU Address Info detected" << std::endl;
                return getFruAddressInfo(reqdata);
            case VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD:
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Get Mandatory Sensor Numbers detected" << std::endl;
                return getDeviceLocatorRecordId(reqdata);
            default:
                return emptyData;
        };
    } else {
        return emptyData;
    }

}


int VITA4611_ChassisManager::pollDeviceSensors() {
    for (const auto& pair : deviceMap) {
        VITA4611Device* device = pair.second;
        // dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] polling sensor readings for device " << static_cast<int>(device->device_id) << std::endl;
        device->getAllSensorReadings();
    }
    return 1;
}

int VITA4611_ChassisManager::pollSDRs() {
    for (const auto& pair : deviceMap) {
        VITA4611Device* device = pair.second;
        if (device->get_dynamic_sensors() == 1){
            device->checkSDRInfo();
        }
    }
    return 1;
}

void VITA4611_ChassisManager::exposeFruSensors() {
    log<level::INFO>("[ENTRY] VITA4611 Exposing Sensors for all FRUs");
    for (const auto& pair : deviceMap) {
        VITA4611Device* device = pair.second;
        uint8_t num = pair.first;
        for (SensorRecord& sensor : device->fruinfo.sensors) {
            
            log<level::INFO>(("LCR:IPMID[ENTRY] device " + std::to_string(static_cast<int>(device->device_id)) + \
            ", sensor " + std::to_string(static_cast<int>(sensor.sensorNumber)) + ", name " + sensor.SDR_Name + ", reading " + std::to_string(static_cast<int>(sensor.raw_reading)) + \
            " " + sensor.SDR_Units).c_str());
            
            std::string path = "/xyz/openbmc_project/sensors/";
            std::string sanitizedName = sensor.SDR_Name;
            
            std::replace(sanitizedName.begin(), sanitizedName.end(), '-', '_');
            
            std::string sensorKey = sanitizedName + "_" + std::to_string(static_cast<int>(num));
            
            try {
                if (sensor.SDR_Units == "degrees C") {
                    path = path + "temperature/" + sensorKey;
                    log<level::INFO>("LCR:IPMID[ENTRY] Temperature Units");
                    sensor.sensorIface = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());
                    sensor.sensorIface->value(sensor.reading_converted);
                    sensor.sensorIface->unit(ValueIface::Unit::DegreesC);
                    sensor.sensorIface->criticalAlarmHigh(false);
                    sensor.sensorIface->criticalAlarmLow(false);
                    sensor.sensorIface->criticalHigh(sensor.converted_upper_critical_threshold);
                    sensor.sensorIface->criticalLow(sensor.converted_lower_critical_threshold);
                } else if (sensor.SDR_Units == "Volts") {
                    path = path + "voltage/" + sensorKey;
                    log<level::INFO>("LCR:IPMID[ENTRY] Volts Units");
                    sensor.sensorIface = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());
                    sensor.sensorIface->value(sensor.reading_converted);
                    sensor.sensorIface->unit(ValueIface::Unit::Volts);
                    sensor.sensorIface->criticalAlarmHigh(false);
                    sensor.sensorIface->criticalAlarmLow(false);
                    sensor.sensorIface->criticalHigh(sensor.converted_upper_critical_threshold);
                    sensor.sensorIface->criticalLow(sensor.converted_lower_critical_threshold);
                } else if (sensor.SDR_Units == "Amps") {
                    path = path + "current/" + sensorKey;
                    log<level::INFO>("LCR:IPMID[ENTRY] Amps Units");
                    sensor.sensorIface = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());
                    sensor.sensorIface->value(sensor.reading_converted);
                    sensor.sensorIface->unit(ValueIface::Unit::Amperes);
                    sensor.sensorIface->criticalAlarmHigh(false);
                    sensor.sensorIface->criticalAlarmLow(false);
                    sensor.sensorIface->criticalHigh(sensor.converted_upper_critical_threshold);
                    sensor.sensorIface->criticalLow(sensor.converted_lower_critical_threshold);
                } else if (sensor.SDR_Units == "Watts") {
                    path = path + "power/" + sensorKey;
                    log<level::INFO>("LCR:IPMID[ENTRY] Watts Units");
                    sensor.sensorIface = std::make_unique<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>(bus, path.c_str());
                    sensor.sensorIface->value(sensor.reading_converted);
                    sensor.sensorIface->unit(ValueIface::Unit::Watts);
                    sensor.sensorIface->criticalAlarmHigh(false);
                    sensor.sensorIface->criticalAlarmLow(false);
                    sensor.sensorIface->criticalHigh(sensor.converted_upper_critical_threshold);
                    sensor.sensorIface->criticalLow(sensor.converted_lower_critical_threshold);
                } else {
                    path = path + "unknown/" + sensorKey;
                    log<level::INFO>(("LCR:IPMID[ENTRY] Unknown Units at path " + path).c_str());
                }
            } catch (const sdbusplus::exception::SdBusError& e) {
                log<level::ERR>(("Failed to create D-Bus sensor object at path " + path + ": " + e.what()).c_str());
            } catch (const std::exception& e) {
                log<level::ERR>(("Unexpected error creating sensor object '" + sanitizedName + "': " + e.what()).c_str());
            } catch (...) {
                log<level::ERR>("Unknown error creating sensor object - continuing");
            }
        }
        //start to update the sensor readings now that the interfaces have been established
        device->getAllSensorReadings();
    }
}

/*
* ipmitool command handler
* ipmitool calls as follows
    ipmitool raw 0x3A 1 <ipmi address> netfn lun cmd
*
* This command handler validates the ipmi address to ensure it is a
* device present on the bus. If the device with the ipmi address is
* not present, this function will return a failure

* Then the response from the device is sent as is.
* 
*/
ipmi::RspType<std::vector<uint8_t>>
VITA4611_ChassisManager::execute_passthrough_cmd(const std::vector<uint8_t>& reqdata)
{
    uint8_t ipmi_address;
    uint8_t ch = 0;

    if (m_ipmb_a_enable == 1) {
        ch = 0;
    } else if (m_ipmb_a_enable == 0 && m_ipmb_b_enable == 1) {
        ch = 4;
    } else {
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] no channels available";
        return ipmi::responseIllegalCommand();
    }

    std::vector<uint8_t> ChMResponseData;

    if (reqdata.size() < 4) {
        dbgFile << "LCR:IPMID. ERROR. Invalid number of parameters " << (int)(reqdata.size()) << std::endl; 
        return ipmi::responseReqDataLenInvalid();
    }
    dbgFile.flush();
    ipmi_address = reqdata[0];
    uint8_t netFn = reqdata[1];
    uint8_t lun = reqdata[2];
    uint8_t cmd = reqdata[3];

    /* if ipmi-address is 0x40
    *  then it matches us and return the data
    */
    if (ipmi_address == CHMC_IPMI_ADDRESS) {

        ChMResponseData = handleChMCommand(reqdata);
        if (!ChMResponseData.empty()) {
            dbgFile << "LCR:IPMID. ERROR. response data received" << std::endl;
            return ipmi::responseSuccess(ChMResponseData);
        } else {
            dbgFile << "LCR:IPMID. ERROR. response data empty" << std::endl; 
            return ipmi::responseIllegalCommand();
        }
    }
    else {
        std::optional<ipmb_response_return_type_t> ipmb_response;

        /*
        * Verify the ipmi address is valid
        */
        if (false == verify_ipmi_address_is_valid(ipmi_address)) {
            return ipmi::responseIllegalCommand();
        }

        VITA4611Device *pdev = findDeviceByAddress(ipmi_address);

        if (NETFN_VITA4611 == netFn) {
            if (cmd == VITA4611_GETVSOCAPS_CMD) {
                std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
                uint8_t ipmi_response_cc;
                dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] getting device by address" << std::endl;
                
                if (pdev) {
                    /* payload should start with the VSO identifier
                    *  followed by the user payload
                    *  we know reqdata vector is at least 4 elements long
                    */
                    
                    payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                    ipmb_response = sendIPMBRequest(
                        ch,
                        ipmi_address,
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc,
                        false);
                    if (ipmb_response.has_value()) {
                        ipmb_response_return_type_t resp;
                        resp = ipmb_response.value();
                        return ipmi::responseSuccess(std::get<5>(resp));
                    }
                    else {
                        return ipmi::responseCmdFailInitAgent();
                    }
                }
                else {
                    return ipmi::responseInvalidCommandOnLun();
                }
            }
            else if (cmd == VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD) {
                std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
                uint8_t ipmi_response_cc;
                if (pdev) {
                    /* payload should start with the VSO identifier
                    *  followed by the user payload
                    *  we know reqdata vector is at least 4 elements long
                    */
                    payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                    ipmb_response = sendIPMBRequest(
                        ch,
                        ipmi_address,
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc,
                        false);
                    if (ipmb_response.has_value()) {
                        ipmb_response_return_type_t resp;
                        resp = ipmb_response.value();
                        return ipmi::responseSuccess(std::get<5>(resp));
                    }
                    else {
                        return ipmi::response(ipmi_response_cc);
                    }
                }
                else {
                    return ipmi::responseInvalidCommandOnLun();
                } 
            }
            else {
                std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier
                uint8_t ipmi_response_cc;
                //VITA4611Device *p = deviceMap[0];
                if (pdev) {
                    /* payload should start with the VSO identifier
                    *  followed by the user payload
                    *  we know reqdata vector is at least 4 elements long
                    */
                    payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                    ipmb_response = sendIPMBRequest(
                        ch,
                        ipmi_address,
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc,
                        false);
                    if (ipmb_response.has_value()) {
                        ipmb_response_return_type_t resp;
                        resp = ipmb_response.value();
                        return ipmi::responseSuccess(std::get<5>(resp));
                    }
                    else {
                        return ipmi::response(ipmi_response_cc);
                    }
                }
                else {
                    return ipmi::responseInvalidCommandOnLun();
                }
            }
        }
        else {
            /* pass the netfn and lun and cmd unfiltered.
            *  This is for normal ipmi commands and
            *  responses
            */
            std::vector<uint8_t> payload;
            uint8_t ipmi_response_cc;
            //VITA4611Device *p = deviceMap[0];
            if (pdev) {
                /* payload should start with the VSO identifier
                *  followed by the user payload
                *  we know reqdata vector is at least 4 elements long
                */
                payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                ipmb_response = sendIPMBRequest(
                    ch,
                    ipmi_address,
                    netFn,
                    lun,
                    cmd,
                    payload,
                    &ipmi_response_cc,
                    false);
                if (ipmb_response.has_value()) {
                    ipmb_response_return_type_t resp;
                    resp = ipmb_response.value();
                    return ipmi::responseSuccess(std::get<5>(resp));
                }
                else {
                    return ipmi::response(ipmi_response_cc);
                }
            }
        }
    }

    return ipmi::responseUnspecifiedError();
}
