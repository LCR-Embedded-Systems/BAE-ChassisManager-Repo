/*
#include "config.h"

#include "user_channel/channel_layer.hpp"

#include <ipmid/api.hpp>
#include <ipmid/utils.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Network/EthernetInterface/server.hpp>
#include "ipmid/api-types.hpp"
    
#include <bitset>
#include <cmath>
#include <fstream>
#include <variant>
#include <vector>
#include <string>
#include "vita4611/vita4611cm.hpp"
#include <fstream>
#include <ostream>
*/
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

    ipmi_address_list.push_back(SHMC_IPMI_ADDRESS);

    getChassisManager().get_device_list(ipmi_address_list);
    return ipmi::responseSuccess(ipmi_address_list);
}


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
            "failed" << std::endl;
    }
    else {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmddevicelist <<
            "SUCCESS" << std::endl;        
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
            "failed" << std::endl;
    }
    else {
        dbgFile << "LCR:IPMI: Registering handler for " << (int)ipmi::LCR::netFnLCR << 
            " " << (int)ipmi::LCR::netFnLCRcmdpassthrough <<
            "SUCCESS" << std::endl;        
    }

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