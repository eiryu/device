#ifndef __BLE112_H__
#define __BLE112_H__

#include "BGLib.h"

// imported from firmware/ble112/attributes.txt
#define ATTRIBUTE_HANDLE_IR_DATA              17 // 0x11
#define ATTRIBUTE_HANDLE_IR_UNREAD_STATUS     20 // 0x14
#define ATTRIBUTE_HANDLE_IR_CONTROL_POINT     24 // 0x18
#define ATTRIBUTE_HANDLE_IR_CARRIER_FREQUENCY 27 // 0x1B
#define ATTRIBUTE_HANDLE_IR_AUTH_STATUS       30 // 0x1E

#define NEXT_COMMAND_ID_ENCRYPT_START               0x01
#define NEXT_COMMAND_ID_USER_WRITE_RESPONSE_SUCCESS 0x02
#define NEXT_COMMAND_ID_EMPTY                       0xFF

#define INVALID_BOND_HANDLE 0xFF

class BLE112 {
    public:
        BLE112(HardwareSerial *module);
        uint8 nextCommand;
        uint8 currentBondHandle;

        void setup();
        void loop();
        void reset();
        void hello();
        void startAdvertising();
        void getRSSI();
        void writeAttribute();
        void writeAttributeAuthorizationStatus(bool);
        void writeAttributeUnreadStatus(bool);
        void readAttribute();
        void disconnect();
        void encryptStart();
        void getBonds();
        void passkeyEntry();
        void setOobData();
        void deleteBonding(uint8);
        void attributesUserReadResponse();
        void attributesUserReadResponseData(uint8, uint8, uint8*);
        void attributesUserReadResponseAuthorized(bool);
        void attributesUserReadResponseFrequency(uint16);
        void attributesUserWriteResponse(uint8, uint8);
        void incrementReceivedCount();
        void updateAdvData();

    private:
        // create BGLib object:
        //  - use SoftwareSerial por for module comms
        //  - use nothing for passthrough comms (0 = null pointer)
        //  - enable packet mode on API protocol since flow control is unavailable
        BGLib bglib;
        uint8 _receivedCount;

        void setAdvData(uint8, uint8*);
        void gapSetMode(uint8, uint8);
        void gapSetAdvParameters(uint16, uint16, uint8);
        void smSetBondableMode();
        void smSetParameters();
};

#endif
