/*
 * Copyright (c) Pigeon Point Systems. All right reserved
 */

#pragma once

/* VITA 46.11 commands.
*  LCR specific notes.
*  These commands are all needed for the highest level of
*  Tier (Tier3) that the LCR openBmc supports
*  for both chassis manager and IPMC
*/
#define VITA_GET_VSO_CAPABILITIES_CMD		    0x00
#define VITA_GET_CHASSIS_ADDRESS_TABLE_INFO_CMD 0x1
#define VITA_GET_CHASSIS_IDENTIFIER_CMD         0x2
#define VITA_SET_CHASSIS_IDENTIFIER_CMD         0x3
#define VITA_FRU_CONTROL_CMD			0x04
#define VITA_GET_FRU_LED_PROPERTIES_CMD		0x05
#define VITA_GET_LED_COLOR_CAPABILITIES_CMD	0x06
#define VITA_SET_FRU_LED_STATE_CMD		0x07
#define VITA_GET_FRU_LED_STATE_CMD		0x08
#define VITA_SET_IPMB_STATE_CMD             0x9
#define VITA_SET_FRU_STATE_POLICY_BITS_CMD	0x0A
#define VITA_GET_FRU_STATE_POLICY_BITS_CMD	0x0B
#define VITA_SET_FRU_ACTIVATION_CMD		0x0C
#define VITA_GET_DEVICE_LOCATOR_RECORD_ID_CMD   0xD
#define VITA_GET_CHASSIS_MGR_IPMB_ADDR_CMD      0x1B
#define VITA_SET_FAN_POLICY_CMD                 0x1C
#define VITA_GET_FAN_POLICY_CMD                 0x1D
#define VITA_FRU_CONTROL_CAPS_CMD               0x1E
#define VITA_FRU_INVENTORY_DEVICE_LOCK_CTRL_CMD 0x1F
#define VITA_FRU_INVENTORY_DEVICE_WRITE_CMD     0x20
#define VITA_GET_CHASSIS_MANAGER_IP_ADDR_CMD    0x21
#define VITA_GET_FRU_ADDRESS_INFO_CMD		0x40
#define VITA_GET_MANDATORY_SENSOR_NUMBERS_CMD   0x44
#define VITA_FRU_HASH_CMD                       0x45
#define VITA_GET_PAYLOAD_MODE_CAPS_CMD          0x46
#define VITA_SET_PAYLOAD_MODE_CMD               0x47
#define VITA_GET_POWER_SUPPLY_CAPS_CMD          0x4b
#define VITA_GET_POWER_SUPPLY_STATUS_CMD        0x4c
#define VITA_SET_POWER_SUPPLY_STATUS_CMD        0x4D

/* VITA 46.11 site types */
#define VITA_FRONT_VPX_MODULE		0x00
#define VITA_POWER_ENTRY		0x01
#define VITA_CHASSIS_FRU		0x02
#define VITA_DEDICATED_CHMC		0x03
#define VITA_FAN_TRAY			0x04
#define VITA_FAN_TRAY_FILTER		0x05
#define VITA_ALARM_PANEL		0x06
#define VITA_XMC			0x07
#define VITA_VPX_RTM			0x09
#define VITA_FRONT_VME_MODULE		0x0A
#define VITA_FRONT_VXS_MODULE		0x0B
#define VITA_POWER_SUPPLY		0x0C
#define VITA_FRONT_VITA62_MODULE	0x0D
#define VITA_71_MODULE			0x0E
#define VITA_FMC			0x0F


#define GROUP_EXT_VITA		0x03

extern uint8_t
vita_discover(struct ipmi_intf *intf);

extern uint8_t
ipmi_vita_ipmb_address(struct ipmi_intf *intf);

extern int
ipmi_vita_main(struct ipmi_intf * intf, int argc, char ** argv);
