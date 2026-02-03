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
//testing testing testing once again testing
//extern std::ofstream dbgFile;



typedef std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>> ipmb_response_return_type_t;

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
    const uint8_t netfn, 
    const uint8_t lun, 
    const uint8_t cmd, 
    std::vector<uint8_t> &data,
    uint8_t *ipmi_response_cc /* out */
)
{
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID: Sending IPMB request with the following data: " << std::endl;
    dbgFile << "\t channel: " << static_cast<int>(channel) << std::endl;
    dbgFile << "\t netfn: " << static_cast<int>(netfn) << std::endl;
    dbgFile << "\t lun: " << static_cast<int>(lun) << std::endl;
    dbgFile << "\t cmd: " << static_cast<int>(cmd) << std::endl;
    dbgFile << "\t data: ";
    for (const auto& byte : data)
    {
        dbgFile << "0x" << std::hex << std::uppercase << static_cast<int>(byte) << " ";
    }
    dbgFile << std::dec << std::nouppercase << std::endl; // Reset stream formatting

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
        method.append(channel, netfn, lun, cmd, data);

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
VITA4611Device::queryDeviceId()
{
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID querying device ID" << std::endl;
    std::optional<ipmb_response_return_type_t> ipmb_response;
    std::vector<uint8_t> payload; // Get Device ID has no payload
    uint8_t ipmi_response_cc;
    ipmb_response = sendIPMBRequest(
        0,
        IPMI_GET_DEVICE_ID_NETFN,
        0,
        IPMI_GET_DEVICE_ID_CMD,
        payload,
        &ipmi_response_cc
    );
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp;
        resp = ipmb_response.value();

        DeviceIDData = std::get<5>(resp);
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID Bytes of the device ID ";
        for (const auto& byte : DeviceIDData)
        {
            dbgFile << static_cast<int>(byte) << " ";  
        }
        dbgFile << "\n";
        validDeviceID = true;
    }
    else {
        validDeviceID = false;
    }
}

void
VITA4611Device::enumerateFRUSensors(uint8_t fru)
{
    std::optional<ipmb_response_return_type_t> ipmb_response;
    uint8_t ipmi_response_cc;
    std::vector<uint8_t> payload = {0x03, (uint8_t) fru}; // VSO Group Extension Identifier
    FruInfo fruinfo;
    SensorRecord rec;
    ipmb_response = 
        sendIPMBRequest(
            0,
            NETFN_VITA4611,
            0,
            VITA4611_GET_MANDATORY_SENSOR_NUMBERS_CMD,
            payload,
            &ipmi_response_cc);
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
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_State;
       fruinfo.sensors.push_back(rec);

       rec.sensorNumber = resp_payload[2];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Health;
       fruinfo.sensors.push_back(rec);

       rec.sensorNumber = resp_payload[3];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Voltage;
       fruinfo.sensors.push_back(rec);

       rec.sensorNumber = resp_payload[4];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_Temp;
       fruinfo.sensors.push_back(rec);

       rec.sensorNumber = resp_payload[5];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_PayloadTestResults;
       fruinfo.sensors.push_back(rec);

       rec.sensorNumber = resp_payload[6];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_PayloadTestStatus;
       fruinfo.sensors.push_back(rec);

       /* 7 is reserved */

       rec.sensorNumber = resp_payload[8];
       rec.sensorType = VITA4611FRUSensorType::FRUSensor_Type_PayloadModeSensor;
       fruinfo.sensors.push_back(rec);
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
   for (i = 0; i < vsocaps->MaxFRUDeviceID; i++) {
    enumerateFRUSensors(i);
   }
}



void
VITA4611Device::queryVsoCapabilities()
{
    std::optional<ipmb_response_return_type_t> ipmb_response;
    uint8_t ipmi_response_cc;
    dbgFile << "LCR:IPMID::ENTRY " << __FUNCTION__ << " " << std::endl;

    std::vector<uint8_t> payload = {0x03}; // VSO Group Extension Identifier

    ipmb_response = sendIPMBRequest(channel, NETFN_VITA4611, 0, VITA4611_GETVSOCAPS_CMD, payload, &ipmi_response_cc);
    if (ipmb_response.has_value()) {
        ipmb_response_return_type_t resp;
        resp = ipmb_response.value();

        dbgFile << "\n";
        isVITA4611Device = true;

        vsocaps = new VSOCaps;
        *vsocaps = *((struct VSOCaps *) std::get<5>(resp).data());
        dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Device is a vita 46.11 device" << std::endl;
        dbgFile << "VSOIdentifier:      " << static_cast<int>(vsocaps->VSOIdentifier) << "\n";
        dbgFile << "IPMCIdentifier:     " << static_cast<int>(vsocaps->IPMCIdentifier) << "\n";
        dbgFile << "IPMBCapabilities:   " << static_cast<int>(vsocaps->IPMBCapabilities) << "\n";
        dbgFile << "VSOStandard:        " << static_cast<int>(vsocaps->VSOStandard) << "\n";
        dbgFile << "VSORevision:        " << static_cast<int>(vsocaps->VSORevision) << "\n";
        dbgFile << "MaxFRUDeviceID:     " << static_cast<int>(vsocaps->MaxFRUDeviceID) << "\n";
        dbgFile << "IPMCFRUDeviceID:    " << static_cast<int>(vsocaps->IPMCFRUDeviceID) << std::endl;
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
    queryDeviceId();

    if (validDeviceID == true) {
        dbgFile <<"LCR:IPMID. Device implements device id cmd. Querying VSO Capabilities" << std::endl;
        queryVsoCapabilities();

        if (isVITA4611Device == true) {
            /* discover and build the FRU map for this device */
            dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID. Device is a vita 46.11 device" << std::endl;
            discoverFRUs();
        }
    }
}

/*
* Implementation of VITA4611_ChassisManager
* This creates a thread that services requests
* from ipmitool or over RMCP+ or other channels.
*/

VITA4611_ChassisManager::VITA4611_ChassisManager(): bus(sdbusplus::bus::new_default())
{
    dbgFile << "[" << currentTimestamp() << "] "  << "LCR:IPMI::Constructor" << std::endl;
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

    int n = 0;
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
           if ( addr == SHMC_I2C_ADDRESS ) {
                continue;
           }

           if (write(file, nullptr, 0) >= 0) {
                /*
                * add the device
                */
               dbgFile << 
                "LCR::IPMID::Detected i2c device " <<
                "on bus " << (int)bus << " at addr " << (int) addr << std::endl;
               i2cdeviceMap[bus].push_back({bus, addr, bus, 0x20});
               deviceMap[n] = new VITA4611Device(0, addr << 1);
               n += 1;
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

   return responseData;
}

void VITA4611_ChassisManager::getAddTable() {
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Calling get Add table" << std::endl;
    for (const auto& [key, device] : deviceMap)
    {
        dbgFile << "\tLCR:IPMID[ENTRY] IPMB address: " << static_cast<int>(device->ipmi_address) << std::endl;
        dbgFile << "\tLCR:IPMID[ENTRY] Hardware address: " << static_cast<int>(device->hw_address) << std::endl;
        dbgFile << "\tLCR:IPMID[ENTRY] Device ID: " << static_cast<int>(key) << std::endl;
        dbgFile << "\tLCR:IPMID[ENTRY] Site Number: " << static_cast<int>(device->site_number) << std::endl;
        dbgFile << "\tLCR:IPMID[ENTRY] Site Type: " << static_cast<int>(device->site_type) << std::endl;
    }
    dbgFile.flush();
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

std::vector<uint8_t> VITA4611_ChassisManager::handleChMCommand(const std::vector<uint8_t>& reqdata) {
    //uint8_t ipmi_address = reqdata[0];
    uint8_t netFn = reqdata[1];
    //uint8_t lun = reqdata[2];
    uint8_t cmd = reqdata[3];
    
    std::vector<uint8_t> responseData60 = {
        0x01,
        0x01,
        0x01,
        0x02,
        0x02,
        0x02,
    };
    std::vector<uint8_t> emptyData = {};
    if ((netFn == NETFN_VITA4611) && (cmd ==VITA4611_GETVSOCAPS_CMD)) {
        return getShMCVSOCaps();
    } 
    else if ((netFn == NETFN_VITA4611) && (cmd == 60)) {
        getAddTable();
        return responseData60;
    }
    else {
        return emptyData;
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
    std::vector<uint8_t> ChMResponseData;
    dbgFile << "[" << currentTimestamp() << "] " << "LCR:IPMID[ENTRY] Pass through command with arguments: ";

    for (const auto& byte : reqdata)
    {
        dbgFile << static_cast<int>(byte) << " ";
    }
    dbgFile << "\n";

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
    if (ipmi_address == SHMC_IPMI_ADDRESS) {

        ChMResponseData = handleChMCommand(reqdata);
        if (!ChMResponseData.empty()) {
            return ipmi::responseSuccess(ChMResponseData);
        } else {
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
                
                //VITA4611Device *p = deviceMap[0];
                if (pdev) {
                    /* payload should start with the VSO identifier
                    *  followed by the user payload
                    *  we know reqdata vector is at least 4 elements long
                    */
                    
                    payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                    ipmb_response = sendIPMBRequest(
                        ch,
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc);
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
                //VITA4611Device *p = deviceMap[0];
                if (pdev) {
                    /* payload should start with the VSO identifier
                    *  followed by the user payload
                    *  we know reqdata vector is at least 4 elements long
                    */
                    payload.insert(payload.end(), reqdata.begin() + 4, reqdata.end());
                    ipmb_response = sendIPMBRequest(
                        ch,
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc);
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
                        netFn,
                        lun,
                        cmd,
                        payload,
                        &ipmi_response_cc);
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
                    netFn,
                    lun,
                    cmd,
                    payload,
                    &ipmi_response_cc);
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
