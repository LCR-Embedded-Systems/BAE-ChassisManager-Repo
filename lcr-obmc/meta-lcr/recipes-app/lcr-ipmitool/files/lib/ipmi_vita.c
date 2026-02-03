/*
 * Copyright (c) 2014 Pigeon Point Systems. All right reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of Pigeon Point Systems, or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS, " without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * PIGEON POINT SYSTEMS ("PPS") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * PPS OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF PPS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */


#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_picmg.h>
#include <ipmitool/ipmi_vita.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/log.h>

/* Handled VITA 46.11 commands */
#define VITA_CMD_HELP		0
#define VITA_CMD_PROPERTIES	1
#define VITA_CMD_FRUCONTROL	2
#define VITA_CMD_ADDRINFO	3
#define VITA_CMD_ACTIVATE	4
#define VITA_CMD_DEACTIVATE	5
#define VITA_CMD_POLICY_GET	6
#define VITA_CMD_POLICY_SET	7
#define VITA_CMD_LED_PROP	8
#define VITA_CMD_LED_CAP	9
#define VITA_CMD_LED_GET	10
#define VITA_CMD_LED_SET	11
#define VITA_CMD_GET_DEVICE_LIST 12

/* Tier2 and Tier 3 chassis manager commands */
#define VITA_CMD_GET_CHASSIS_ADDRESS_TABLE_INFO	12
#define VITA_CMD_GET_CHASSIS_ID		13
#define VITA_CMD_SET_CHASSIS_ID		14
#define VITA_CMD_SET_IPMB_STATE		15
#define VITA_CMD_SET_FRU_POLICY_STATE_BITS	16
#define VITA_CMD_GET_FRU_POLICY_STATE_BITS	17
#define VITA_CMD_GET_DEVICE_LOCATOR_ID		18
#define VITA_CMD_GET_CHASSIS_MGR_IPMB_ADDRESS	19
#define VITA_CMD_SET_FAN_POLICY		20
#define VITA_CMD_GET_FAN_POLICY		21
#define VITA_CMD_GET_CHASSIS_MGR_IP_ADDR	22
#define VITA_CMD_GET_SENSOR_NUMBERS		23
#define VITA_CMD_GET_FRU_HASH			24
#define VITA_CMD_GET_PAYLOAD_CAPABILITIES	25
#define VITA_CMD_SET_PAYLOAD_MODE			26
#define VITA_CMD_GET_POWER_SUPPLY_CAPABILITIES	27
#define VITA_CMD_GET_POWER_SUPPLY_STATUS	28
#define VITA_CMD_SET_POWER_SUPPLY_STATUS	29
#define VITA_CMD_GET_MANDATORY_SENSOR_INFORMATION 30
#define VITA_CMD_UNKNOWN	255

#define LCR_SHMC_I2C_ADDRESS 	0x20
#define LCR_SHMC_IPMI_ADDRESS 	(LCR_SHMC_I2C_ADDRESS << 1)

/* VITA 46.11 Site Type strings */
static struct valstr vita_site_types[] = {
	{ VITA_FRONT_VPX_MODULE, "Front Loading VPX Plug-In Module" },
	{ VITA_POWER_ENTRY, "Power Entry Module" },
	{ VITA_CHASSIS_FRU, "Chassic FRU Information Module" },
	{ VITA_DEDICATED_CHMC, "Dedicated Chassis Manager" },
	{ VITA_FAN_TRAY, "Fan Tray" },
	{ VITA_FAN_TRAY_FILTER, "Fan Tray Filter" },
	{ VITA_ALARM_PANEL, "Alarm Panel" },
	{ VITA_XMC, "XMC" },
	{ VITA_VPX_RTM, "VPX Rear Transition Module" },
	{ VITA_FRONT_VME_MODULE, "Front Loading VME Plug-In Module" },
	{ VITA_FRONT_VXS_MODULE, "Front Loading VXS Plug-In Module" },
	{ VITA_POWER_SUPPLY, "Power Supply" },
	{ VITA_FRONT_VITA62_MODULE, "Front Loading VITA 62 Module\n" },
	{ VITA_71_MODULE, "VITA 71 Module\n" },
	{ VITA_FMC, "FMC\n" },
	{ 0, NULL }
};

/* VITA 46.11 command help strings */
static struct valstr vita_help_strings[] = {
	{
		VITA_CMD_HELP,
		"VITA commands:\n"
		"    properties        - get VSO properties\n"
		"    frucontrol        - FRU control\n"
		"    addrinfo          - get address information\n"
		"    activate          - activate a FRU\n"
		"    deactivate        - deactivate a FRU\n"
		"    policy get        - get the FRU activation policy\n"
		"    policy set        - set the FRU activation policy\n"
		"    led prop          - get led properties\n"
		"    led cap           - get led color capabilities\n"
		"    led get           - get led state\n"
		"    led set           - set led state\n"
		"    getchassisid	   - Get Chassis ID\n"
		"    setchassisid	   - Set Chassis ID\n"
		"    setipmbstate      - Set IPMB state\n"
		"    getmandatorysensor      - Get Mandatory sensors\n"
		"    getaddrlist      - Get list of device ipmi addresses\n"
	},
	{
		VITA_CMD_FRUCONTROL,
		"usage: frucontrol <FRU-ID> <OPTION>\n"
		"    OPTION: 0 - Cold Reset\n"
		"            1 - Warm Reset\n"
		"            2 - Graceful Reboot\n"
		"            3 - Issue Diagnostic Interrupt"
	},
	{
		VITA_CMD_GET_DEVICE_LIST,
		"usage: getaddrlist\n"
	},
	{
		VITA_CMD_ADDRINFO,
		"usage: addrinfo [<FRU-ID>]"
	},
	{
		VITA_CMD_ACTIVATE,
		"usage: activate <FRU-ID>"
	},
	{
		VITA_CMD_DEACTIVATE,
		"usage: deactivate <FRU-ID>"
	},
    	{
		VITA_CMD_POLICY_GET,
		"usage: policy get <FRU-ID>"
	},
	{
		VITA_CMD_POLICY_SET,
		"usage: policy set <FRU-ID> <MASK> <VALUE>\n"
		"    MASK:  [3] affect the Default-Activation-Locked Policy Bit\n"
		"           [2] affect the Commanded-Deactivation-Ignored Policy Bit\n"
		"           [1] affect the Deactivation-Locked Policy Bit\n"
		"           [0] affect the Activation-Locked Policy Bit\n"
		"    VALUE: [3] value for the Default-Activation-Locked Policy Bit\n"
		"           [2] value for the Commanded-Deactivation-Ignored Policy Bit\n"
		"           [1] value for the Deactivation-Locked Policy Bit\n"
		"           [0] value for the Activation-Locked Policy Bit"
	},
	{
		VITA_CMD_LED_PROP,
		"usage: led prop <FRU-ID>"
	},
	{
		VITA_CMD_LED_CAP,
		"usage: led cap <FRU-ID> <LED-ID"
	},
	{
		VITA_CMD_LED_GET,
		"usage: led get <FRU-ID> <LED-ID",
	},
	{
		VITA_CMD_LED_SET,
		"usage: led set <FRU-ID> <LED-ID> <FUNCTION> <DURATION> <COLOR>\n"
		"    <FRU-ID>\n"
		"    <LED-ID>   0-0xFE:    Specified LED\n"
		"               0xFF:      All LEDs under management control\n"
		"    <FUNCTION> 0:       LED OFF override\n"
		"               1 - 250: LED blinking override (off duration)\n"
		"               251:     LED Lamp Test\n"
		"               252:     LED restore to local control\n"
		"               255:     LED ON override\n"
		"    <DURATION> 1 - 127: LED Lamp Test / on duration\n"
		"    <COLOR>    1:   BLUE\n"
		"               2:   RED\n"
		"               3:   GREEN\n"
		"               4:   AMBER\n"
		"               5:   ORANGE\n"
		"               6:   WHITE\n"
		"               0xE: do not change\n"
		"               0xF: use default color"
	},
	{
		VITA_CMD_GET_CHASSIS_ID,
		"Usage: getchassisid",
	},
	{
		VITA_CMD_SET_CHASSIS_ID,
		"Usage: setchassisid <Sequence of bytes>",
	},
	{
		VITA_CMD_SET_IPMB_STATE,
		"Usage: setipmbstate Byte0 Byte1 Byte2",
	},
	{
		VITA_CMD_GET_MANDATORY_SENSOR_INFORMATION,
		"Usage: GetMandatorySensor frudeviceid",
	},
	{
		VITA_CMD_UNKNOWN,
		"Unknown command"
	},
	{ 0, NULL }
};

/*
* get vso capabilities
* If an address is provided, we will use the address
* if not, this command will be routed to the chassis
* manager
*/
uint8_t
vita_discover(struct ipmi_intf *intf)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	unsigned char msg_data[4];
	int vita_avail = 0;

	memset(&req, 0, sizeof(req));

	/* mark the main netfn as passthrough netfn*/
	req.msg.netfn = IPMI_NETFN_LCR_PASSTHROUGH;
	req.msg.cmd = IPMI_LCR_CMD_PASSTHROUGH;

	/*
	* The message is 4 bytes
	* target_address
	* 0x2c and lun is 0
	* cmd == 0
	* The openbmc implementation of this command
	* will add the GROUP_VITA (0x3) to the payload
	*/
	if (true == intf->target_addr_valid) {
		msg_data[0] = (intf->target_addr << 1);
	}
	else {
		msg_data[0] = LCR_SHMC_IPMI_ADDRESS; /* BMC ipmi address */
	}
	msg_data[1] = IPMI_NETFN_PICMG;
	msg_data[2] = 0;
	msg_data[3] = VITA_GET_VSO_CAPABILITIES_CMD;
	req.msg.data = &msg_data[0];
	req.msg.data_len = 4;

	lprintf(LOG_INFO, "Running Get VSO Capabilities my_addr %#x, "
		"transit %#x, target %#x",
		intf->my_addr, intf->transit_addr, intf->target_addr);

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received");
	} else if (rsp->ccode == 0xC1) {
		lprintf(LOG_INFO, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
	} else if (rsp->ccode == 0xCC) {
		lprintf(LOG_INFO, "Invalid data field received: %s",
			val2str(rsp->ccode, completion_code_vals));
	} else if (rsp->ccode) {
		lprintf(LOG_INFO, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
	} else if (rsp->data_len < 5) {
		lprintf(LOG_INFO, "Invalid response length %d",
			rsp->data_len);
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_INFO, "Invalid group extension %#x",
			rsp->data[0]);
	} else if ((rsp->data[3] & 0x03) != 0) {
		lprintf(LOG_INFO, "Unknown VSO Standard %d",
			(rsp->data[3] & 0x03));
	} else if ((rsp->data[4] & 0x0F) != 1) {
		lprintf(LOG_INFO, "Unknown VSO Specification Revision %d.%d",
			(rsp->data[4] & 0x0F), (rsp->data[4] >> 4));
	} else {
		vita_avail = 1;
		lprintf(LOG_INFO, "Discovered VITA 46.11 Revision %d.%d",
			(rsp->data[4] & 0x0F), (rsp->data[4] >> 4));
	}

	return vita_avail;
}

uint8_t
ipmi_vita_ipmb_address(struct ipmi_intf *intf)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	unsigned char msg_data;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_FRU_ADDRESS_INFO_CMD;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;

	msg_data = GROUP_EXT_VITA;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received");
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
	} else if (rsp->data_len < 7) {
		lprintf(LOG_ERR, "Invalid response length %d",
			rsp->data_len);
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x",
			rsp->data[0]);
	} else {
		return rsp->data[2];
	}

	return 0;
}

static int
ipmi_vita_getaddr(struct ipmi_intf *intf, int argc, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[2];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_FRU_ADDRESS_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = GROUP_EXT_VITA;		/* VITA identifier */
	msg_data[1] = 0;			/* default FRU ID */

	if (argc > 0) {
		/* validate and get FRU Device ID */
		if (is_fru_id(argv[0], &msg_data[1]) != 0) {
			return -1;
		}
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 7) {
		lprintf(LOG_ERR, "Invalid response length %d",
			rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x",
			rsp->data[0]);
		return -1;
	}

	printf("Hardware Address : 0x%02x\n", rsp->data[1]);
	printf("IPMB-0 Address   : 0x%02x\n", rsp->data[2]);
	printf("FRU ID           : 0x%02x\n", rsp->data[4]);
	printf("Site ID          : 0x%02x\n", rsp->data[5]);
	printf("Site Type        : %s\n", val2str(rsp->data[6],
		vita_site_types));
	if (rsp->data_len > 8) {
		printf("Channel 7 Address: 0x%02x\n", rsp->data[8]);
	}

	return 0;
}

/*
* ipmi_vita_set_ipmb_state
* Sets the ipmb state
* Caller must be provide 3 bytes
*/
static int
ipmi_vita_set_ipmb_state(struct ipmi_intf *intf, int num_bytes, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[4];
	unsigned int byte;

	memset(&req, 0, sizeof(req));

	/* we cannot use 0 bytes as identifier for vita 46.11 */
	if (num_bytes != 3) {
		lprintf(LOG_ERR, "Please provide IPMB-A state byte IPMB-B state byte and IPMB speed");
		return -1;
	}

	msg_data[0] = GROUP_EXT_VITA;
	sscanf(argv[0], "%x", &byte);
	msg_data[1] = (unsigned char) byte;

	sscanf(argv[1], "%x", &byte);
	msg_data[2] = (unsigned char) byte;

	sscanf(argv[2], "%x", &byte);
	msg_data[3] = (unsigned char) byte;

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_SET_IPMB_STATE_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}
	else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received. %s",
		val2str(rsp->ccode, completion_code_vals));
		return -1;
	}	
	else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	return 0;
}

/*
* Implementation of the set chassis id command
* for Tier 3 chassis managers and IPMC
* Parameters
*  intf :- IPMI interface
*  num_bytes :- number of bytes in the identifier
*  argv :- An array of bytes to use for the identifier
*
*  Send a VITA 46.11 request in the following format 
*  First byte is VSO identifier (value of 3)
*  Second byte is VITA spec (3) and # of bytes in identifier
*  Remaining bytes are the chassis identifier

*  returns 0 on success
*  Returns -1 on failure and log it
*/

static int
ipmi_vita_set_chassis_id(struct ipmi_intf *intf, int num_bytes, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char *msg_data;
	int i;
	unsigned int byte;

	memset(&req, 0, sizeof(req));

	/* we cannot use 0 bytes as identifier for vita 46.11 */
	if (!num_bytes) {
		lprintf(LOG_ERR, "# of bytes in chassis identifier is %d (not valid)\n", num_bytes);
		return -1;
	}

	/* allocate the data.
	*  This is 2 bytes more than the actual identifier
	*  See the vita 46.11 specification
	*  First byte in request is 
	*/
	msg_data = (unsigned char *) malloc((num_bytes+2) * sizeof(unsigned char));
	if (!msg_data) {
		lprintf(LOG_ERR, "unable to allocate memory");
		return -1;
	}

	/* create the data buffer See Set Chassis identifier command in vita specification
	*
	*/
    msg_data[0] = GROUP_EXT_VITA;		/* VITA identifier */
	msg_data[1] = (0xc0) | (num_bytes & 0x3f);
	for (i = 0; i < num_bytes; i++) {
		sscanf(argv[i], "%x", &byte);
		msg_data[i+2] = (unsigned char)byte;
	}

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_SET_CHASSIS_IDENTIFIER_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = num_bytes+2;	/* Actual data is 2 bytes more */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}
	else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received. %s",
		val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length. %d", rsp->data_len);
		return -1;
	}
	else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	return 0;
}


/*
* Implements the get_chassis_id vita 46.11 command
* code is 2
* Returns 0 if successful and logs values
* else -1 if failure
*/
static int
ipmi_vita_get_chassis_id(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data;
	uint8_t chassis_id_length_byte;
	int i;
	uint8_t chassis_id_length_in_bytes = 0;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_CHASSIS_IDENTIFIER_CMD;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;

	msg_data = GROUP_EXT_VITA;		/* VITA identifier */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	}
	else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received. %s",
		val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	else if (rsp->data_len < 3) {
		lprintf(LOG_ERR, "Invalid response length. %d", rsp->data_len);
		return -1;
	}
	else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	chassis_id_length_byte = rsp->data[1];
	if ( (chassis_id_length_byte & 0xc0 ) != 0xc0) {
		lprintf(LOG_ERR, "Not a VITA 46.11 chassis %d", chassis_id_length_byte);
		return -1;
	}

	chassis_id_length_in_bytes = chassis_id_length_byte & 0x3f;

	printf("# of bytes in chassis identifier : %d\n", chassis_id_length_in_bytes);
	for (i = 0; i < chassis_id_length_in_bytes; i++) {
		printf("Byte %d in id is 0x%x\n", i, rsp->data[2+i]);
	}
	return 0;
}

static int
ipmi_vita_get_vso_capabilities(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data, tmp;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_VSO_CAPABILITIES_CMD;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;

	msg_data = GROUP_EXT_VITA;		/* VITA identifier */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 5) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("VSO Identifier    : 0x%02x\n", rsp->data[0]);
	printf("IPMC Identifier   : 0x%02x\n", rsp->data[1]);
	printf("    Tier  %d\n", (rsp->data[1] & 0x03) + 1);
	printf("    Layer %d\n", ((rsp->data[1] & 0x30) >> 4) + 1);

	printf("IPMB Capabilities : 0x%02x\n", rsp->data[2]);

	tmp = (rsp->data[2] & 0x30) >> 4;

	printf("    Frequency  %skHz\n",
		tmp == 0 ? "100" : tmp == 1 ? "400" : "RESERVED");

	tmp = rsp->data[2] & 3;

	if (tmp == 1) {
		printf("    2 IPMB interfaces supported\n");
	} else if (tmp == 0) {
		printf("    1 IPMB interface supported\n");
	}

	printf("VSO Standard      : %s\n",
		(rsp->data[3] & 0x3) == 0 ? "VITA 46.11" : "RESERVED");

	printf("VSO Spec Revision : %d.%d\n", rsp->data[4] & 0xf,
		rsp->data[4] >> 4);

	printf("Max FRU Device ID : 0x%02x\n", rsp->data[5]);
	printf("FRU Device ID     : 0x%02x\n", rsp->data[6]);

	return 0;
}

static int
ipmi_vita_get_mandatory_sensors(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[5];

	memset(&req, 0, sizeof(req));

	/* mark the main netfn as passthrough netfn*/
	req.msg.netfn = IPMI_NETFN_LCR_PASSTHROUGH;
	req.msg.cmd = IPMI_LCR_CMD_PASSTHROUGH;

	/* if the user provided a target address
	*  use the address
	*  convert it into a ipmi address before sending it
	* else use the address 0x40 (ShMC i2c address is 0x20 and
	*  ipmi address is 0x40)
	*/
	if (true == intf->target_addr_valid) {
		lprintf(LOG_NOTICE, "target address provided\n");
		msg_data[0] = (intf->target_addr << 1);
	}
	else {
		lprintf(LOG_NOTICE, "target address not provided\n");
		msg_data[0] = LCR_SHMC_IPMI_ADDRESS; /* BMC ipmi address */
	}

	msg_data[1] = IPMI_NETFN_PICMG;
	msg_data[2] = 0;
	msg_data[3] = VITA_GET_MANDATORY_SENSOR_NUMBERS_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 5;

	if (is_fru_id(argv[0], &msg_data[4]) != 0) {	/* FRU ID */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	return 0;	
}

static int
ipmi_vita_set_fru_activation(struct ipmi_intf *intf,
	char **argv, unsigned char command)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	/* mark the main netfn as passthrough netfn*/
	req.msg.netfn = IPMI_NETFN_LCR_PASSTHROUGH;
	req.msg.cmd = IPMI_LCR_CMD_PASSTHROUGH;

	/* if the user provided a target address
	*  use the address
	*  convert it into a ipmi address before sending it
	* else use the address 0x40 (ShMC i2c address is 0x20 and
	*  ipmi address is 0x40)
	*/
	if (true == intf->target_addr_valid) {
		msg_data[0] = (intf->target_addr << 1);
	}
	else {
		msg_data[0] = LCR_SHMC_IPMI_ADDRESS; /* BMC ipmi address */
	}

	msg_data[1] = IPMI_NETFN_PICMG;
	msg_data[2] = 0;
	msg_data[3] = VITA_FRU_CONTROL_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	if (is_fru_id(argv[0], &msg_data[4]) != 0) {	/* FRU ID */
		return -1;
	}

	msg_data[5]	= command;			/* command */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("FRU has been successfully %s\n",
		command ? "activated" : "deactivated");

	return 0;
}



static int
ipmi_vita_get_fru_state_policy_bits(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[2];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_FRU_STATE_POLICY_BITS_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 2) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("FRU State Policy Bits:	%xh\n", rsp->data[1]);
	printf("    Default-Activation-Locked Policy Bit is %d\n",
		rsp->data[1] & 0x08 ? 1 : 0);
	printf("    Commanded-Deactivation-Ignored Policy Bit is %d\n",
		rsp->data[1] & 0x04 ? 1 : 0);
	printf("    Deactivation-Locked Policy Bit is %d\n",
		rsp->data[1] & 0x02 ? 1 : 0);
	printf("    Activation-Locked Policy Bit is %d\n",
		rsp->data[1] & 0x01);

	return 0;
}

static int
ipmi_vita_set_fru_state_policy_bits(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[4];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_SET_FRU_STATE_POLICY_BITS_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}
	if (str2uchar(argv[1], &msg_data[2]) != 0) {	/* bits mask */
		return -1;
	}
	if (str2uchar(argv[2], &msg_data[3]) != 0) {	/* bits */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("FRU state policy bits have been updated\n");

	return 0;
}

static int
ipmi_vita_get_led_properties(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[2];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_FRU_LED_PROPERTIES_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 3) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("LED Count:	   %#x\n", rsp->data[2]);

	return 0;
}

static int
ipmi_vita_get_led_color_capabilities(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[3];
	int i;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_LED_COLOR_CAPABILITIES_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}
	if (str2uchar(argv[1], &msg_data[2]) != 0) {	/* LED-ID */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 4) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("LED Color Capabilities: ");
	for (i = 0; i < 8; i++) {
		if (rsp->data[1] & (0x01 << i)) {
			printf("%s, ", picmg_led_color_str(i));
		}
	}
	putchar('\n');

	printf("Default LED Color in\n");
	printf("      LOCAL control:  %s\n", picmg_led_color_str(rsp->data[2]));
	printf("      OVERRIDE state: %s\n", picmg_led_color_str(rsp->data[3]));

	if (rsp->data_len == 5) {
		printf("LED flags:\n");
		if (rsp->data[4] & 2) {
			printf("      [HW RESTRICT]\n");
		}
		if (rsp->data[4] & 1) {
			printf("      [PAYLOAD PWR]\n");
		}
	}

	return 0;
}

static int
ipmi_vita_get_led_state(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[3];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_GET_FRU_LED_STATE_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}
	if (str2uchar(argv[1], &msg_data[2]) != 0) {	/* LED-ID */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 5
		|| ((rsp->data[1] & 0x2) && rsp->data_len < 8)
		|| ((rsp->data[1] & 0x4) && rsp->data_len < 9)) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("LED states:                   %x\t", rsp->data[1]);
	if (rsp->data[1] & 0x1) {
		printf("[LOCAL CONTROL] ");
	}
	if (rsp->data[1] & 0x2) {
		printf("[OVERRIDE] ");
	}
	if (rsp->data[1] & 0x4) {
		printf("[LAMPTEST] ");
	}
	if (rsp->data[1] & 0x8) {
		printf("[HW RESTRICT] ");
	}
	putchar('\n');

	if (rsp->data[1] & 1) {
		printf("  Local Control function:     %x\t", rsp->data[2]);
		if (rsp->data[2] == 0x0) {
			printf("[OFF]\n");
		} else if (rsp->data[2] == 0xff) {
			printf("[ON]\n");
		} else {
			printf("[BLINKING]\n");
		}
		printf("  Local Control On-Duration:  %x\n", rsp->data[3]);
		printf("  Local Control Color:        %x\t[%s]\n",
			rsp->data[4], picmg_led_color_str(rsp->data[4] & 7));
	}

	/* override state or lamp test */
	if (rsp->data[1] & 0x06) {
		printf("  Override function:     %x\t", rsp->data[5]);
		if (rsp->data[5] == 0x0) {
			printf("[OFF]\n");
		} else if (rsp->data[5] == 0xff) {
			printf("[ON]\n");
		} else {
			printf("[BLINKING]\n");
		}
		printf("  Override On-Duration:  %x\n", rsp->data[6]);
		printf("  Override Color:        %x\t[%s]\n",
			rsp->data[7], picmg_led_color_str(rsp->data[7] & 7));
		if (rsp->data[1] == 0x04) {
			printf("  Lamp test duration:    %x\n", rsp->data[8]);
		}
	}

	return 0;
}

static int
ipmi_vita_set_led_state(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = VITA_SET_FRU_LED_STATE_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	msg_data[0] = GROUP_EXT_VITA;			/* VITA identifier */
	if (is_fru_id(argv[0], &msg_data[1]) != 0) {	/* FRU ID */
		return -1;
	}
	if (str2uchar(argv[1], &msg_data[2]) != 0) {	/* LED-ID */
		return -1;
	}
	if (str2uchar(argv[2], &msg_data[3]) != 0) {	/* LED function */
		return -1;
	}
	if (str2uchar(argv[3], &msg_data[4]) != 0) {	/* LED on duration */
		return -1;
	}
	if (str2uchar(argv[4], &msg_data[5]) != 0) {	/* LED color */
		return -1;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("LED state has been updated\n");

	return 0;
}

static int
ipmi_get_device_list(struct ipmi_intf *intf, int buf_len, uint8_t *addr_list)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data;
	int i;

	memset(&req, 0, sizeof(req));

	/* mark the main netfn as passthrough netfn*/
	req.msg.netfn = IPMI_NETFN_LCR_PASSTHROUGH;
	req.msg.cmd = IPMI_LCR_CMD_GETLISTOFIPMIADDRESSES;
	req.msg.data = &msg_data;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received for get device list");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	}
	else {
		/* the response data contains addresses used */
		for (i = 0; i < rsp->data_len; i++) {
			printf("IPMI address received = 0x%x\n", rsp->data[i]);
		}

		/* return the addresses */
		if (buf_len && addr_list) {
			for (i = 0; i < buf_len; i++) {
				if (i == rsp->data_len) break;
				addr_list[i] = rsp->data[i];
			}
		}
		return 0;
	}
}

/*
* ipmi_vita_fru_control
* uses LCR passthrough implementation
* to send the command to the LCR ShMC
*/
static int
ipmi_vita_fru_control(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	/* mark the main netfn as passthrough netfn*/
	req.msg.netfn = IPMI_NETFN_LCR_PASSTHROUGH;
	req.msg.cmd = IPMI_LCR_CMD_PASSTHROUGH;

	/* if the user provided a target address
	*  use the address
	*  convert it into a ipmi address before sending it
	* else use the address 0x40 (ShMC i2c address is 0x20 and
	*  ipmi address is 0x40)
	*/
	if (true == intf->target_addr_valid) {
		msg_data[0] = (intf->target_addr << 1);
	}
	else {
		msg_data[0] = LCR_SHMC_IPMI_ADDRESS; /* BMC ipmi address */
	}

	msg_data[1] = IPMI_NETFN_PICMG;
	msg_data[2] = 0;
	msg_data[3] = VITA_FRU_CONTROL_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	if (is_fru_id(argv[0], &msg_data[4]) != 0) {	/* FRU ID */
		return -1;
	}
	if (str2uchar(argv[1], &msg_data[5]) != 0) {	/* control option */
		return -1;
	}

	printf("FRU Device Id: %d FRU Control Option: %s\n", msg_data[4],
		val2str(msg_data[5], picmg_frucontrol_vals));

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "No valid response received.");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Invalid completion code received: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len < 1) {
		lprintf(LOG_ERR, "Invalid response length %d", rsp->data_len);
		return -1;
	} else if (rsp->data[0] != GROUP_EXT_VITA) {
		lprintf(LOG_ERR, "Invalid group extension %#x", rsp->data[0]);
		return -1;
	}

	printf("FRU Control: ok\n");

	return 0;
}

static int
ipmi_vita_get_cmd(int argc, char **argv)
{
	if (argc < 1 || !strcmp(argv[0], "help")) {
		return VITA_CMD_HELP;
	}

	/* Get VSO Properties */
	if (!strcmp(argv[0], "properties")) {
		return VITA_CMD_PROPERTIES;
	}

	/* get chassis address table info */
	if (!strcmp(argv[0], "chassisaddr")) {
		return VITA_CMD_GET_CHASSIS_ADDRESS_TABLE_INFO;
	}

	/* get chassis identifier */
	if (!strcmp(argv[0], "getchassisid")) {
		return VITA_CMD_GET_CHASSIS_ID;
	}

	/* set chassis identifier */
	if (!strcmp(argv[0], "setchassisid")) {
		return VITA_CMD_SET_CHASSIS_ID;
	}

	/* FRU Control command */
	if (!strcmp(argv[0], "frucontrol")) {
		return VITA_CMD_FRUCONTROL;
	}

	if (!strcmp(argv[0], "getaddrlist")) {
		return VITA_CMD_GET_DEVICE_LIST;
	}

	/* set ipmb state */
	if (!strcmp(argv[0], "setipmbstate")) {
		return VITA_CMD_SET_IPMB_STATE;
	}

	/* set fru state policy */
	if (!strcmp(argv[0], "setfrupolicy")) {
		return VITA_CMD_SET_FRU_POLICY_STATE_BITS;
	}

	/* get fru state policy */
	if (!strcmp(argv[0], "getfrupolicy")) {
		return VITA_CMD_GET_FRU_POLICY_STATE_BITS;
	}

	if (!strcmp(argv[0], "getdevicelocator")) {
		return VITA_CMD_GET_DEVICE_LOCATOR_ID;
	}

	/* get chassis manager ipmb address */
	if (!strcmp(argv[0], "getcmipmbaddr")) {
		return VITA_CMD_GET_CHASSIS_MGR_IPMB_ADDRESS;
	}

	/* set fan policy */
	if (!strcmp(argv[0], "setfanpolicy")) {
		return VITA_CMD_SET_FAN_POLICY;
	}

	if (!strcmp(argv[0], "getfanpolicy")) {
		return VITA_CMD_GET_FAN_POLICY;
	}

	if (!strcmp(argv[0], "getmandatorysensors")) {
		return VITA_CMD_GET_MANDATORY_SENSOR_INFORMATION;
	}

	/* Get FRU Address Info command */
	if (!strcmp(argv[0], "addrinfo")) {
		return VITA_CMD_ADDRINFO;
	}

	/* Set FRU Activation (activate) command */
	if (!strcmp(argv[0], "activate")) {
		return VITA_CMD_ACTIVATE;
	}

	/* Set FRU Activation (deactivate) command */
	if (!strcmp(argv[0], "deactivate")) {
		return VITA_CMD_DEACTIVATE;
	}

	/* FRU State Policy Bits commands */
	if (!strcmp(argv[0], "policy")) {
		if (argc < 2) {
			return VITA_CMD_UNKNOWN;
		}

		/* Get FRU State Policy Bits command */
		if (!strcmp(argv[1], "get")) {
			return VITA_CMD_POLICY_GET;
		}

		/* Set FRU State Policy Bits command */
		if (!strcmp(argv[1], "set")) {
			return VITA_CMD_POLICY_SET;
		}

		/* unknown command */
		return VITA_CMD_UNKNOWN;
	}

	/* FRU LED commands */
	if (!strcmp(argv[0], "led")) {
		if (argc < 2) {
			return VITA_CMD_UNKNOWN;
		}

		/* FRU LED Get Properties */
		if (!strcmp(argv[1], "prop")) {
			return VITA_CMD_LED_PROP;
		}

		/* FRU LED Get Capabilities */
		if (!strcmp(argv[1], "cap")) {
			return VITA_CMD_LED_CAP;
		}

		/* FRU LED Get State */
		if (!strcmp(argv[1], "get")) {
			return VITA_CMD_LED_GET;
		}

		/* FRU LED Set State */
		if (!strcmp(argv[1], "set")) {
			return VITA_CMD_LED_SET;
		}

		/* unknown command */
		return VITA_CMD_UNKNOWN;
	}

	/* unknown command */
	return VITA_CMD_UNKNOWN;
}

int
ipmi_vita_main (struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = -1, show_help = 0;
	int cmd = ipmi_vita_get_cmd(argc, argv);

	switch (cmd) {
	case VITA_CMD_HELP:
		cmd = ipmi_vita_get_cmd(argc - 1, &argv[1]);
		show_help = 1;
		rc = 0;
		break;

	case VITA_CMD_PROPERTIES:
		rc = ipmi_vita_get_vso_capabilities(intf);
		break;

	case VITA_CMD_GET_DEVICE_LIST:
		rc = ipmi_get_device_list(intf, 0, NULL);
		break;

	case VITA_CMD_FRUCONTROL:
		if (argc > 2) {
			rc = ipmi_vita_fru_control(intf, &argv[1]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_ADDRINFO:
		rc = ipmi_vita_getaddr(intf, argc - 1, &argv[1]);
		break;

	case VITA_CMD_ACTIVATE:
		if (argc > 1) {
			rc = ipmi_vita_set_fru_activation(intf, &argv[1], 1);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_DEACTIVATE:
		if (argc > 1) {
			rc = ipmi_vita_set_fru_activation(intf, &argv[1], 0);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_POLICY_GET:
		if (argc > 2) {
			rc = ipmi_vita_get_fru_state_policy_bits(intf,
				&argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_POLICY_SET:
		if (argc > 4) {
			rc = ipmi_vita_set_fru_state_policy_bits(intf,
				&argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_LED_PROP:
		if (argc > 2) {
			rc = ipmi_vita_get_led_properties(intf, &argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_LED_CAP:
		if (argc > 3) {
			rc = ipmi_vita_get_led_color_capabilities(intf,
				&argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_LED_GET:
		if (argc > 3) {
			rc = ipmi_vita_get_led_state(intf, &argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_LED_SET:
		if (argc > 6) {
			rc = ipmi_vita_set_led_state(intf, &argv[2]);
		} else {
			show_help = 1;
		}
		break;

	case VITA_CMD_GET_CHASSIS_ID:
		rc = ipmi_vita_get_chassis_id(intf);
		break;

	case VITA_CMD_SET_CHASSIS_ID:
		rc = ipmi_vita_set_chassis_id(intf, argc-1, &argv[1]);
		break;

	case VITA_CMD_SET_IPMB_STATE:
		rc = ipmi_vita_set_ipmb_state(intf, argc-1, &argv[1]);
		break;

	case VITA_CMD_GET_MANDATORY_SENSOR_INFORMATION:
		if (argc > 1) {
			rc = ipmi_vita_get_mandatory_sensors(intf, &argv[1]);
		}
		else {
			show_help = 1;
		}
		break;

	default:
		lprintf(LOG_NOTICE, "Unknown command");
		cmd = VITA_CMD_HELP;
		show_help = 1;
		break;
	}

	if (show_help) {
		lprintf(LOG_NOTICE, "%s", val2str(cmd, vita_help_strings));
	}

	return rc;
}
