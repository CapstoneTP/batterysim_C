#include "all_headers.h"

/*================================================================
dbc.c
reset whole structures in dbc.h
=================================================================*/


// Reset (ID: 1568) || "SMARANSY"
BMS_Company_Info_t bms_company_info = {
    {'S', 'M', 'A', 'R', 'A', 'N', 'S', 'Y'}
};

// Reset (ID: 1569) || VIN:"JNHEL43B"
VIN_car_info_t vin_car_info = {
    {'J', 'N', 'H', 'E', 'L', '4', '3', 'B'}
};

// Reset (ID: 1570)
BMS_Status_t bms_status = {
    .Status = 0x00,
    .Time = 0x1234,
    .Flags = 0x00,
    .DTC1 = 0b00000001,
    .DTC2 = 0b00000010
};

// Reset (ID: 1571)
BMS_Battery_Info_t bms_battery_info = {
    .Voltage = 0x2400,
    .MinVoltage = 0x10,
    .MinVoltageID = 0x01,
    .MaxVoltage = 0x32,
    .MaxVoltageID = 0x02
};

// Reset (ID: 1572)
BMS_Charge_Current_Limits_t bms_charge_current_limits = {
    .Current = 0x0000,
    .ChargeLimit = 0x0000,
    .DischargeLimit = 0x0000
};

// Reset (ID: 1574)
BMS_SOC_t bms_soc = {
    .SOC = 0x7F,
    .DOD = 0x0000,
    .Capacity = 0x0000,
    .SOH = 0x7F
};

// Reset (ID: 1575)
BMS_Temperature_t bms_temperature = {
    .Temperature = 0x00,
    .AirTemp = 0x00,
    .MinTemp = 0x00,
    .MinTempID = 0x00,
    .MaxTemp = 0x00,
    .MaxTempID = 0x00
};

// Reset (ID: 1576)
BMS_Resistance_t bms_resistance = {
    .Resistance = 0x0000,
    .MinResistance = 0x00,
    .MinResistanceID = 0x00,
    .MaxResistance = 0x00,
    .MaxResistanceID = 0x00
};

// Reset (ID: 1577)
BMS_DC_Charging_t bms_dc_charging = {
    .DCLineVoltage = 0x0000,
    .DCLineCurrent = 0x0000,
    .MaxChargeCurrent = 0x00,
    .MaxDischargeCurrent = 0x00,
    .DCLinePower = 0x0000
};