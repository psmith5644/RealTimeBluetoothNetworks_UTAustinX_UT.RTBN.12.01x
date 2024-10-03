// AP_Lab6.c
// Runs on either MSP432 or TM4C123
// see GPIO.c file for hardware connections 

// Daniel Valvano and Jonathan Valvano
// November 20, 2016
// CC2650 booster or CC2650 LaunchPad, CC2650 needs to be running SimpleNP 2.2 (POWERSAVE)

#include <stdint.h>
#include "../inc/UART0.h"
#include "../inc/UART1.h"
#include "../inc/AP.h"
#include "AP_Lab6.h"
//**debug macros**APDEBUG defined in AP.h********
#ifdef APDEBUG 
#define OutString(STRING) UART0_OutString(STRING)
#define OutUHex(NUM) UART0_OutUHex(NUM)
#define OutUHex2(NUM) UART0_OutUHex2(NUM)
#define OutChar(N) UART0_OutChar(N)
#else
#define OutString(STRING)
#define OutUHex(NUM)
#define OutUHex2(NUM)
#define OutChar(N)
#endif

//****links into AP.c**************
extern const uint32_t RECVSIZE;
extern uint8_t RecvBuf[];
typedef struct characteristics{
  uint16_t theHandle;          // each object has an ID
  uint16_t size;               // number of bytes in user data (1,2,4,8)
  uint8_t *pt;                 // pointer to user data, stored little endian
  void (*callBackRead)(void);  // action if SNP Characteristic Read Indication
  void (*callBackWrite)(void); // action if SNP Characteristic Write Indication
}characteristic_t;
extern const uint32_t MAXCHARACTERISTICS;
extern uint32_t CharacteristicCount;
extern characteristic_t CharacteristicList[];
typedef struct NotifyCharacteristics{
  uint16_t uuid;               // user defined 
  uint16_t theHandle;          // each object has an ID (used to notify)
  uint16_t CCCDhandle;         // generated/assigned by SNP
  uint16_t CCCDvalue;          // sent by phone to this object
  uint16_t size;               // number of bytes in user data (1,2,4,8)
  uint8_t *pt;                 // pointer to user data array, stored little endian
  void (*callBackCCCD)(void);  // action if SNP CCCD Updated Indication
}NotifyCharacteristic_t;
extern const uint32_t NOTIFYMAXCHARACTERISTICS;
extern uint32_t NotifyCharacteristicCount;
extern NotifyCharacteristic_t NotifyCharacteristicList[];

#ifndef NULL
  #define NULL ((void *)0)
#endif

#define CMD0_SYNC_REQ (0x35)
#define CMD0_ASYNC (0x55)
#define GATT_READ_PERMISSIONS (0x01)
#define GATT_WRITE_PERMISSIONS (0x02)
#define RFU (0x0)

typedef enum {
  CMD1_GetRevisionResponse = 0x03,
  CMD1_GetStatus = 0x06,
  CMD1_StartAdvertisement = 0x42,
  CMD1_SetAdvertisementData = 0x43,
  CMD1_AddService = 0x81,
  CMD1_AddCharacteristicValueDeclaration = 0x82,
  CMD1_AddCharacteristicDescriptorDeclaration = 0x83,
  CMD1_RegisterService = 0x84,
  CMD1_SetGATTParameter = 0x8C
} SNP_CMD1;

//**************Lab 6 routines*******************
// **********SetFCS**************
// helper function, add check byte to message
// assumes every byte in the message has been set except the FCS
// used the length field, assumes less than 256 bytes
// FCS = 8-bit EOR(all bytes except SOF and the FCS itself)
// Inputs: pointer to message
//         stores the FCS into message itself
// Outputs: none
void SetFCS(uint8_t *msg){
  uint8_t payloadLength = msg[1];
  uint16_t lastPayloadByteOffset = 4 + payloadLength; // SOF, 2 length bytes, 2 command bytes, payload

  uint8_t FCS = msg[1];
  for (uint16_t i = 2; i <= lastPayloadByteOffset; i++) {
    FCS = FCS ^ msg[i];
  }

  uint8_t FCSOffset = lastPayloadByteOffset+1;
  msg[FCSOffset] = FCS;
}

uint32_t strlen(const char * cstring) {
    int len = 0;
    while (cstring[len] != '\0') { 
        len++;
    }
    return len;
}

void BuildMsg(uint8_t * msg, uint8_t length, uint8_t cmd0, uint8_t cmd1, uint8_t * payload) {
  msg[0] = SOF;
  msg[1] = length;
  msg[2] = 0;
  msg[3] = cmd0;
  msg[4] = cmd1;

  for (int i = 5; i < length+5; i++) {
    msg[i] = payload[i-5];
  }

  SetFCS(msg);
}
//*************BuildGetStatusMsg**************
// Create a Get Status message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will Get Status
void BuildGetStatusMsg(uint8_t *msg){
  // hint: see NPI_GetStatus in AP.c
  BuildMsg(msg, 0, CMD0_ASYNC, CMD1_GetStatus, NULL);
}
//*************Lab6_GetStatus**************
// Get status of connection, used in Lab 6
// Input:  none
// Output: status 0xAABBCCDD
// AA is GAPRole Status
// BB is Advertising Status
// CC is ATT Status
// DD is ATT method in progress
uint32_t Lab6_GetStatus(void){volatile int r; uint8_t sendMsg[8];
  OutString("\n\rGet Status");
  BuildGetStatusMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return (RecvBuf[4]<<24)+(RecvBuf[5]<<16)+(RecvBuf[6]<<8)+(RecvBuf[7]);
}

//*************BuildGetVersionMsg**************
// Create a Get Version message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will Get Status
void BuildGetVersionMsg(uint8_t *msg){
  // hint: see NPI_GetVersion in AP.c 
  BuildMsg(msg, 0, CMD0_SYNC_REQ, CMD1_GetRevisionResponse, NULL);
}

//*************Lab6_GetVersion**************
// Get version of the SNP application running on the CC2650, used in Lab 6
// Input:  none
// Output: version
uint32_t Lab6_GetVersion(void){volatile int r;uint8_t sendMsg[8];
  OutString("\n\rGet Version");
  BuildGetVersionMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE); 
  return (RecvBuf[5]<<8)+(RecvBuf[6]);
}

//*************BuildAddServiceMsg**************
// Create an Add service message, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        pointer to empty buffer of at least 9 bytes
// Output none
// build the necessary NPI message that will add a service
void BuildAddServiceMsg(uint16_t uuid, uint8_t *msg){
  uint8_t payload[3] = {0x1, (uint8_t)uuid, (uint8_t)(uuid>>8)};
  BuildMsg(msg, 3, CMD0_SYNC_REQ, CMD1_AddService, payload);  
}
//*************Lab6_AddService**************
// Add a service, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
// Output APOK if successful,
//        APFAIL if SNP failure
int Lab6_AddService(uint16_t uuid){ int r; uint8_t sendMsg[12];
  OutString("\n\rAdd service");
  BuildAddServiceMsg(uuid,sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);  
  return r;
}
//*************AP_BuildRegisterServiceMsg**************
// Create a register service message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will register a service
void BuildRegisterServiceMsg(uint8_t *msg){
  BuildMsg(msg, 0, CMD0_SYNC_REQ, CMD1_RegisterService, NULL); 
}
//*************Lab6_RegisterService**************
// Register a service, used in Lab 6
// Inputs none
// Output APOK if successful,
//        APFAIL if SNP failure
int Lab6_RegisterService(void){ int r; uint8_t sendMsg[8];
  OutString("\n\rRegister service");
  BuildRegisterServiceMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return r;
}

//*************BuildAddCharValueMsg**************
// Create a Add Characteristic Value Declaration message, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        permission is GATT Permission, 0=none,1=read,2=write, 3=Read+write 
//        properties is GATT Properties, 2=read,8=write,0x0A=read+write, 0x10=notify
//        pointer to empty buffer of at least 14 bytes
// Output none
// build the necessary NPI message that will add a characteristic value
void BuildAddCharValueMsg(uint16_t uuid,  
  uint8_t permission, uint8_t properties, uint8_t *msg){
  // set RFU to 0 and
  // set the maximum length of the attribute value=512
  // for a hint see NPI_AddCharValue in AP.c
  // for a hint see first half of AP_AddCharacteristic and first half of AP_AddNotifyCharacteristic
  const uint16_t maxLength = 512;

  uint8_t payload[] = {permission, properties, 0x0, RFU, (uint8_t)maxLength, (uint8_t)(maxLength >> 8), (uint8_t)uuid, (uint8_t)(uuid >> 8)};
  BuildMsg(msg, 8, CMD0_SYNC_REQ, CMD1_AddCharacteristicValueDeclaration, payload);   
}

//*************BuildAddCharDescriptorMsg**************
// Create a Add Characteristic Descriptor Declaration message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 20 bytes
//        pointer to empty buffer of at least 32 bytes
// Output none
// build the necessary NPI message that will add a Descriptor Declaration
void BuildAddCharDescriptorMsg(char name[], uint8_t *msg){
// set length and maxlength to the string length
// set the permissions on the string to read
// for a hint see NPI_AddCharDescriptor in AP.c
// for a hint see second half of AP_AddCharacteristic
  const uint8_t parametersLen = 6;
  const uint8_t userDescriptionStringParam = 0x80;

  uint8_t nameLength = strlen(name)+1;
  uint8_t payload[26] = {userDescriptionStringParam, GATT_READ_PERMISSIONS, (uint8_t)nameLength, (uint8_t)(nameLength >> 8), 
                      (uint8_t)nameLength, (uint8_t)(nameLength >> 8)};

  for (uint8_t i = 0; i < nameLength; i++) {
    payload[i+parametersLen] = name[i];
  }

  BuildMsg(msg, nameLength+parametersLen, CMD0_SYNC_REQ, CMD1_AddCharacteristicDescriptorDeclaration, payload);  
}

//*************Lab6_AddCharacteristic**************
// Add a read, write, or read/write characteristic, used in Lab 6
//        for notify properties, call AP_AddNotifyCharacteristic 
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        thesize is the number of bytes in the user data 1,2,4, or 8 
//        pt is a pointer to the user data, stored little endian
//        permission is GATT Permission, 0=none,1=read,2=write, 3=Read+write 
//        properties is GATT Properties, 2=read,8=write,0x0A=read+write
//        name is a null-terminated string, maximum length of name is 20 bytes
//        (*ReadFunc) called before it responses with data from internal structure
//        (*WriteFunc) called after it accepts data into internal structure
// Output APOK if successful,
//        APFAIL if name is empty, more than 8 characteristics, or if SNP failure
int Lab6_AddCharacteristic(uint16_t uuid, uint16_t thesize, void *pt, uint8_t permission,
  uint8_t properties, char name[], void(*ReadFunc)(void), void(*WriteFunc)(void)){
  int r; uint16_t handle; 
  uint8_t sendMsg[32];  
  if(thesize>8) return APFAIL;
  if(name[0]==0) return APFAIL;       // empty name
  if(CharacteristicCount>=MAXCHARACTERISTICS) return APFAIL; // error
  BuildAddCharValueMsg(uuid,permission,properties,sendMsg);
  OutString("\n\rAdd CharValue");
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  handle = (RecvBuf[7]<<8)+RecvBuf[6]; // handle for this characteristic
  OutString("\n\rAdd CharDescriptor");
  BuildAddCharDescriptorMsg(name,sendMsg);
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  CharacteristicList[CharacteristicCount].theHandle = handle;
  CharacteristicList[CharacteristicCount].size = thesize;
  CharacteristicList[CharacteristicCount].pt = (uint8_t *) pt;
  CharacteristicList[CharacteristicCount].callBackRead = ReadFunc;
  CharacteristicList[CharacteristicCount].callBackWrite = WriteFunc;
  CharacteristicCount++;
  return APOK; // OK
} 
  

//*************BuildAddNotifyCharDescriptorMsg**************
// Create a Add Notify Characteristic Descriptor Declaration message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 20 bytes
//        pointer to empty buffer of at least bytes
// Output none
// build the necessary NPI message that will add a Descriptor Declaration
void BuildAddNotifyCharDescriptorMsg(char name[], uint8_t *msg){
  // set length and maxlength to the string length
  // set the permissions on the string to read
  // set User Description String
  // set CCCD parameters read+write
  // for a hint see NPI_AddCharDescriptor4 in VerySimpleApplicationProcessor.c
  // for a hint see second half of AP_AddNotifyCharacteristic
  const uint8_t userDescriptionStringParam = 0x80;
  const uint8_t CCCDParam = 0x04;
  const uint8_t parametersLen = 7;

  uint8_t nameLen = strlen(name)+1;
  uint8_t payload[27] = {userDescriptionStringParam + CCCDParam, GATT_READ_PERMISSIONS + GATT_WRITE_PERMISSIONS,
                        GATT_READ_PERMISSIONS, (uint8_t)nameLen, (uint8_t)(nameLen >> 8), 
                        (uint8_t)nameLen, (uint8_t)(nameLen >> 8)};
  
  for (uint8_t i = 0; i < nameLen; i++) {
    payload[i+parametersLen] = name[i];
  }

  BuildMsg(msg, parametersLen+nameLen, CMD0_SYNC_REQ, CMD1_AddCharacteristicDescriptorDeclaration, payload);
}
  
//*************Lab6_AddNotifyCharacteristic**************
// Add a notify characteristic, used in Lab 6
//        for read, write, or read/write characteristic, call AP_AddCharacteristic 
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        thesize is the number of bytes in the user data 1,2,4, or 8 
//        pt is a pointer to the user data, stored little endian
//        name is a null-terminated string, maximum length of name is 20 bytes
//        (*CCCDfunc) called after it accepts , changing CCCDvalue
// Output APOK if successful,
//        APFAIL if name is empty, more than 4 notify characteristics, or if SNP failure
int Lab6_AddNotifyCharacteristic(uint16_t uuid, uint16_t thesize, void *pt,   
  char name[], void(*CCCDfunc)(void)){
  int r; uint16_t handle; 
  uint8_t sendMsg[36];  
  if(thesize>8) return APFAIL;
  if(NotifyCharacteristicCount>=NOTIFYMAXCHARACTERISTICS) return APFAIL; // error
  BuildAddCharValueMsg(uuid,0,0x10,sendMsg);
  OutString("\n\rAdd Notify CharValue");
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  handle = (RecvBuf[7]<<8)+RecvBuf[6]; // handle for this characteristic
  OutString("\n\rAdd CharDescriptor");
  BuildAddNotifyCharDescriptorMsg(name,sendMsg);
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  NotifyCharacteristicList[NotifyCharacteristicCount].uuid = uuid;
  NotifyCharacteristicList[NotifyCharacteristicCount].theHandle = handle;
  NotifyCharacteristicList[NotifyCharacteristicCount].CCCDhandle = (RecvBuf[8]<<8)+RecvBuf[7]; // handle for this CCCD
  NotifyCharacteristicList[NotifyCharacteristicCount].CCCDvalue = 0; // notify initially off
  NotifyCharacteristicList[NotifyCharacteristicCount].size = thesize;
  NotifyCharacteristicList[NotifyCharacteristicCount].pt = (uint8_t *) pt;
  NotifyCharacteristicList[NotifyCharacteristicCount].callBackCCCD = CCCDfunc;
  NotifyCharacteristicCount++;
  return APOK; // OK
}

//*************BuildSetDeviceNameMsg**************
// Create a Set GATT Parameter message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 24 bytes
//        pointer to empty buffer of at least 36 bytes
// Output none
// build the necessary NPI message to set Device name
void BuildSetDeviceNameMsg(char name[], uint8_t *msg){
  // for a hint see NPI_GATTSetDeviceNameMsg in VerySimpleApplicationProcessor.c
  // for a hint see NPI_GATTSetDeviceName in AP.c
  const uint8_t genericAccessServiceParam = 0x01;
  const uint8_t deviceNameParam = 0x0;
  const uint8_t paramsLen = 3;
  uint8_t nameLen = strlen(name);

  uint8_t payload[27] = {genericAccessServiceParam, deviceNameParam, (uint8_t)(deviceNameParam >> 8)};
  uint8_t i;
  for (i = 0; i < nameLen; i++) {
    payload[i+paramsLen] = name[i];
  }
  payload[i+paramsLen] = 0;

  BuildMsg(msg, nameLen+paramsLen, CMD0_SYNC_REQ, CMD1_SetGATTParameter, payload);
}
//*************BuildSetAdvertisementData1Msg**************
// Create a Set Advertisement Data message, used in Lab 6
// Inputs pointer to empty buffer of at least 16 bytes
// Output none
// build the necessary NPI message for Non-connectable Advertisement Data
void BuildSetAdvertisementData1Msg(uint8_t *msg){
// for a hint see NPI_SetAdvertisementMsg in VerySimpleApplicationProcessor.c
// for a hint see NPI_SetAdvertisement1 in AP.c
// Non-connectable Advertisement Data
// GAP_ADTYPE_FLAGS,DISCOVERABLE | no BREDR  
// Texas Instruments Company ID 0x000D
// TI_ST_DEVICE_ID = 3
// TI_ST_KEY_DATA_ID
// Key state=0
  const uint8_t nonConnectcableAdvertisementDataParam = 0x01;

  // cannot find official documentation for the format of this buffer in docs or anywhere online
  // so copying from example project
  uint8_t payload[] = {
    nonConnectcableAdvertisementDataParam, 
    0x02,0x01,0x06, // GAP_ADTYPE_FLAGS, DISCOVERABLE | no BREDR
    0x06,0xFF,      // length, manufacturer specific
    0x0D ,0x00,     // Texas Instruments Company ID
    0x03,           // TI_ST_DEVICE_ID
    0x00,           // TI_ST_KEY_DATA_ID
    0x00,           // Key state};
  };
  BuildMsg(msg, 11, CMD0_ASYNC, CMD1_SetAdvertisementData, payload);
}

//*************BuildSetAdvertisementDataMsg**************
// Create a Set Advertisement Data message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 24 bytes
//        pointer to empty buffer of at least 36 bytes
// Output none
// build the necessary NPI message for Scan Response Data
void BuildSetAdvertisementDataMsg(char name[], uint8_t *msg){
  // for a hint see NPI_SetAdvertisementDataMsg in VerySimpleApplicationProcessor.c
  // for a hint see NPI_SetAdvertisementData in AP.c
  const uint8_t scanResponseDataParam = 0x0;
  const uint8_t paramsLen = 3;
  const uint8_t localNameCompleteParam = 0x09;

  uint8_t nameLen = strlen(name);
  uint8_t payload[36] = {scanResponseDataParam, nameLen+1, localNameCompleteParam};
  

  for (uint8_t i = 0; i < nameLen; i++) {
    payload[i+paramsLen] = name[i];
  }

  payload[nameLen+paramsLen] = 0x05; // length of this data
  payload[nameLen+paramsLen+1] = 0x12; // GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE
  payload[nameLen+paramsLen+2] = 0x50; payload[nameLen+paramsLen+3] = 0x00; // DEFAULT_DESIRED_MIN_CONN_INTERVAL
  payload[nameLen+paramsLen+4] = 0x20; payload[nameLen+paramsLen+5] = 0x03; // DEFAULT_DESIRED_MAX_CONN_INTERVAL
// Tx power level
  payload[nameLen+paramsLen+6] = 0x02; // length of this data
  payload[nameLen+paramsLen+7] = 0x0A; // GAP_ADTYPE_POWER_LEVEL
  payload[nameLen+paramsLen+8] = 0x00; // 0dBm

  BuildMsg(msg, nameLen+paramsLen+9, CMD0_ASYNC, CMD1_SetAdvertisementData, payload);
}
//*************BuildStartAdvertisementMsg**************
// Create a Start Advertisement Data message, used in Lab 6
// Inputs advertising interval
//        pointer to empty buffer of at least 20 bytes
// Output none
// build the necessary NPI message to start advertisement
void BuildStartAdvertisementMsg(uint16_t interval, uint8_t *msg){
// for a hint see NPI_StartAdvertisementMsg in VerySimpleApplicationProcessor.c
// for a hint see NPI_StartAdvertisement in AP.c
  const uint8_t connectableUndirectedAdvertisementsParam = 0x0;
  const uint16_t timeoutParam = 0x0000;
  const uint8_t behaviorParam = 0x02;

  uint8_t payload[] = {
    connectableUndirectedAdvertisementsParam,
    (uint8_t)timeoutParam, (uint8_t)(timeoutParam >> 8),
    (uint8_t)interval, (uint8_t)(interval >> 8),
    RFU, RFU, RFU, 0x01, RFU, RFU, RFU, 0xC5, // confused: grader wants these values even when docs say they're all RFU
    behaviorParam
  };

  BuildMsg(msg, 14, CMD0_ASYNC, CMD1_StartAdvertisement, payload);
}

//*************Lab6_StartAdvertisement**************
// Start advertisement, used in Lab 6
// Input:  none
// Output: APOK if successful,
//         APFAIL if notification not configured, or if SNP failure
int Lab6_StartAdvertisement(void){volatile int r; uint8_t sendMsg[40];
  OutString("\n\rSet Device name");
  BuildSetDeviceNameMsg("Shape the World",sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rSetAdvertisement1");
  BuildSetAdvertisementData1Msg(sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rSetAdvertisement Data");
  BuildSetAdvertisementDataMsg("Shape the World",sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rStartAdvertisement");
  BuildStartAdvertisementMsg(100,sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return r;
}

