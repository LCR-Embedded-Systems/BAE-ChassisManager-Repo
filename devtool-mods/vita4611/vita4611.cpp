#include "vita4611/vita4611cm.hpp"
using namespace phosphor::logging;

//VITA4611CM vita4611_manager;
std::ofstream dbgFile("lcr.ipmid.log");

VITA4611_ChassisManager& getChassisManager()
{
    static VITA4611_ChassisManager instance;
    return instance;
}

/*
 * GetVSOCapabilities for the chassis manager
 * For the actual devices we need to figure out
 * how to send to the devices
*/

ipmi::RspType<std::vector<uint8_t>>
GetVSOCapabilities(ipmi::Context::ptr&) {
    auto respData = getChassisManager().getShMCVSOCaps();
    auto ret = ipmi::responseSuccess(respData);
    return ret;
}

/*
* lcrcmdpassthrough
* pass through functions for ipmitool when used
* in raw mode with netfn = 0x3F and cmd = 1
* Callers would run with
*   ipmitool raw 0x3F 1 <ipmi address> <parameters>
* 
*/
ipmi::RspType<std::vector<uint8_t>>
lcrcmdpassthrough(ipmi::Context::ptr ctx,
                                     const std::vector<uint8_t>& data)
{
    (void)ctx;
    /* route to the chassis manager
    *  The chassis manager will handle all the commands
    */
    return getChassisManager().execute_passthrough_cmd(data);
}

/*
* lcrgetdevicelist
* Return an array of bytes
* First byte is the ipmi address of the bmc
* all other bytes are ipmi addresses of devices
* we have enumerated on ipmb-A
* All devices enumerated are returned even if
* they are not vita 46.11 devices
*
* By definition this function will return success
*/
ipmi::RspType<std::vector<uint8_t>>
lcrgetdevicelist(ipmi::Context::ptr ctx,
                                     const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> ipmi_address_list;
    (void)ctx;
    (void)data;

    ipmi_address_list.push_back(CHMC_IPMI_ADDRESS);

    getChassisManager().get_device_list(ipmi_address_list);
    return ipmi::responseSuccess(ipmi_address_list);
}



void pollSensors() {
    /*
    TODO
    Rule 5.2.3-2
    The Chassis Manager shall perform periodic polling of the Operational State of all Tier-1 
    FRUs it is managing by issuing a “Get Sensor Reading” IPMI request for the FRU’s FRU 
    State Sensor
    */
    int ret = getChassisManager().pollDeviceSensors();
    if (ret) {;};
}

void pollSDRs() {
    int ret = getChassisManager().pollSDRs();
    if (ret) {;};
}

std::unique_ptr<phosphor::Timer> sensorPollTimer;

// initialize vita4611 state
// This registers group handlers and
// create the VITA 46.11 chassis manager
// object
//
void init_vita4611()
{
    bool bRet;
    log<level::INFO>("[ENTRY] VITA4611::init_vita4611");

    registerGroupHandler(ipmi::prioOpenBmcBase, ipmi::groupVSO,
                         ipmi::vso::cmdGetVSOCapabilities,
                         ipmi::Privilege::User, GetVSOCapabilities);

    /* register the handler for
    *  caller to know all the devices on ipmb-a
    *
    */
    bRet = ipmi::registerHandler(
        ipmi::prioOpenBmcBase,
        ipmi::LCR::netFnLCR,
        ipmi::LCR::netFnLCRcmddevicelist,
        ipmi::Privilege::User,
        lcrgetdevicelist
    );

    if (bRet == false) {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmddevicelist <<
            " failed" << std::endl;
    }
    else {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmddevicelist <<
            " SUCCESS" << std::endl;        
    }

    /*
    * Register our handler for our passthrough
    * functionality. We could not get to
    * intercept ipmitool -t <ipmi address>
    * so we have to implement it this way
    */
    bRet = ipmi::registerHandler(
        ipmi::prioOemBase,
        ipmi::LCR::netFnLCR,
        ipmi::LCR::netFnLCRcmdpassthrough,
        ipmi::Privilege::User,
        lcrcmdpassthrough);

    if (bRet == false) {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmdpassthrough <<
            " failed" << std::endl;
    }
    else {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmdpassthrough <<
            " SUCCESS" << std::endl;        
    }


    /*
    TODO
    Rule 6.3-8
    Support of a Dynamic Sensor Population is a Tier-2 Chassis Manager function. If a Tier 2 Chassis Manager determines that an IPMC supports a Dynamic Sensor Population 
    (based on the IPMC’s response to the “Get Device SDR Info” command), the Chassis 
    Manager shall periodically poll that IPMC with a “Get Device SDR Info” request to 
    monitor for the Sensor Population Change indicator
    */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    pollSensors();
    pollSDRs();
    sensorPollTimer = std::make_unique<phosphor::Timer>( 
        []()
        {
        pollSensors();
        pollSDRs();
        }
    );
    sensorPollTimer->start(std::chrono::seconds(60), true);

    dbgFile.flush();

    log<level::INFO>("[EXIT] VITA4611::init_vita4611");
}

void shutdown_vita4611()
{
    log<level::INFO>("[ENTRY] shutdown_vita4611");
    getChassisManager().sendShutdown();
    log<level::INFO>("[EXIT] shutdown_vita4611");
    dbgFile.flush();
    /* destructor of dbgFile will close it*/
}