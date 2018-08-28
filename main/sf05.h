//==============================================================================
//    S E N S I R I O N   AG,  Laubisruetistr. 50, CH-8712 Staefa, Switzerland
//==============================================================================
// Project   :  SF05 Sample Code (V1.0)
// File      :  sf05.h (V1.0)
// Author    :  RFU
// Date      :  07-Nov-2012
// Controller:  STM32F100RB
// IDE       :  礦ision V4.60.0.0
// Compiler  :  Armcc
// Brief     :  Sensor Layer: Definitions of commands and functions for sensor
//                            access.
//==============================================================================

#ifndef SF05_H
#define SF05_H

//-- Defines -------------------------------------------------------------------
//Processor endian system
//#define BIG ENDIAN   //e.g. Motorola (not tested at this time)
#define LITTLE_ENDIAN  //e.g. PIC, 8051, NEC V850
//==============================================================================
// basic types: making the size of types clear
//==============================================================================
typedef unsigned char   u8t;      ///< range: 0 .. 255
typedef signed char     i8t;      ///< range: -128 .. +127
                                      
typedef unsigned short  u16t;     ///< range: 0 .. 65535
typedef signed short    i16t;     ///< range: -32768 .. +32767
                                      
typedef unsigned long   u32t;     ///< range: 0 .. 4'294'967'295
typedef signed long     i32t;     ///< range: -2'147'483'648 .. +2'147'483'647
                                      
typedef float           ft;       ///< range: +-1.18E-38 .. +-3.39E+38
typedef double          dt;      ///< range:            .. +-1.79E+308

//typedef bool            bt;       ///< values: 0, 1 (real bool used)

typedef union {
  u16t u16;               // element specifier for accessing whole u16
  i16t i16;               // element specifier for accessing whole i16
  struct {
    #ifdef LITTLE_ENDIAN  // Byte-order is little endian
    u8t u8L;              // element specifier for accessing low u8
    u8t u8H;              // element specifier for accessing high u8
    #else                 // Byte-order is big endian
    u8t u8H;              // element specifier for accessing low u8
    u8t u8L;              // element specifier for accessing high u8
    #endif
  } s16;                  // element spec. for acc. struct with low or high u8
} nt16;

typedef union {
  u32t u32;               // element specifier for accessing whole u32
  i32t i32;               // element specifier for accessing whole i32
 struct {
    #ifdef LITTLE_ENDIAN  // Byte-order is little endian
    u16t u16L;            // element specifier for accessing low u16
    u16t u16H;            // element specifier for accessing high u16
    #else                 // Byte-order is big endian
    u16t u16H;            // element specifier for accessing low u16
    u16t u16L;            // element specifier for accessing high u16
    #endif
  } s32;                  // element spec. for acc. struct with low or high u16
} nt32;


//-- Includes ------------------------------------------------------------------
//#include "system.h"
//-- Enumerations --------------------------------------------------------------
// Error codes
typedef enum{
  NO_ERROR       = 0x00, // no error
  ACK_ERROR      = 0x01, // no acknowledgment error
  CHECKSUM_ERROR = 0x02  // checksum mismatch error
}etError;

//-- Enumerations --------------------------------------------------------------
// I2C header
typedef enum{
  I2C_ADR     = 0x40,   // default sensor I2C address
  I2C_WRITE   = 0x00, // write bit in header
  I2C_READ    = 0x01, // read bit in header
  I2C_RW_MASK = 0x01  // bit position of read/write bit in header
}etI2cHeader;

// I2C acknowledge
typedef enum{
  ACK    = 0,
  NO_ACK = 1,
}etI2cAck;

//-- Defines -------------------------------------------------------------------
// CRC
#define POLYNOMIAL  0x131    // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

// masks
#define MASK_STATUS 0x3FFF   // bitmask for the status register
#define MASK_ID     0x0FFF   // bitmask for the id register

//-- Enumerations --------------------------------------------------------------
// Sensor Commands
typedef enum{
  FLOW_MEASUREMENT = 0x1000, // command: flow measurement
  TEMP_MEASUREMENT = 0x1001, // command: temperature measurement
  READ_STATUS      = 0x1010, // command: read status
  READ_ID          = 0x7700, // command: read ID
  SOFT_RESET       = 0X2000  // command: soft reset
}etCommands;
float SF05_GetFlow2(void);//输出流量值

void SF05_APP(void);
#endif
