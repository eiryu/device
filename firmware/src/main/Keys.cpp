#include "Keys.h"
#include <avr/eeprom.h>
#include "pgmStrToRAM.h"
#include "CRC8.h"
#include "Global.h"
#include "convert.h"

#define MORSE_CREDENTIALS_SEPARATOR '/'

//
// DESCRIPTION:
// Fill wifi credentials
//

KeysFiller::KeysFiller ()
    : state(KeysFillerStateSecurity)
{
}

//
// DESCRIPTION:
// Save wifi credentials in EEPROM
//

Keys::Keys()
{
    data = (KeysShared*)gBuffer;
}

void Keys::load()
{
    eeprom_read_block((void*)data,   (void*)0,                  sizeof(KeysShared));
    eeprom_read_block((void*)&data2, (void*)sizeof(KeysShared), sizeof(KeysIndependent));
    if (! isCRCOK()) {
        clear();
    }
}

bool Keys::isCRCOK()
{
    uint8_t crc = crc8( (uint8_t*)data, sizeof(KeysCRCed) );
    return (crc == data->crc8);
}

// crc8 is ok and wifi credentials are valid
bool Keys::isSet()
{
    return data->is_set && data2.is_set;
}

GSwifi::GSSECURITY Keys::getSecurity()
{
    return data->security;
}

const char* Keys::getSSID()
{
    return data->ssid;
}

const char* Keys::getPassword()
{
    return data->password;
}

const char* Keys::getDeviceKey()
{
    return data2.device_key;
}

void Keys::set(GSwifi::GSSECURITY security, const char *ssid, const char *pass)
{
    data->security = security;
    strcpy(data->ssid,     ssid);
    strcpy(data->password, pass);
    data->is_set = true;
}

void Keys::setDeviceKey(const char *key)
{
    strcpy(data2.device_key, key);
    data2.is_set = true;
}

void Keys::save(void)
{
    data->crc8    = crc8( (uint8_t*)data, sizeof(KeysCRCed) );
    eeprom_write_block((const void*)data,   (void*)0,                  sizeof(KeysShared));
    eeprom_write_block((const void*)&data2, (void*)sizeof(KeysShared), sizeof(KeysIndependent));
}

void Keys::clear(void)
{
    data->is_set = false;
    memset( data->ssid,     0, sizeof(data->ssid) );
    memset( data->password, 0, sizeof(data->password) );

    data2.is_set = false;
    memset( data2.device_key, 0, sizeof(data2.device_key) );

    save();

    filler.state = KeysFillerStateSecurity;
    filler.index = 0;
}

// we use morse code to transfer Security, SSID, Password, Device_Key, CRC8 to IRKit device
// SSID can be multi byte, so we transfer HEX 4bit as 1 ASCII character (0-9A-F),
// so we need 2 morse letters to transfer a single character.
int8_t Keys::put(char code)
{
    static uint8_t character;
    static bool is_first_byte;

    if (code == MORSE_CREDENTIALS_SEPARATOR) {
        // wait for putDone() on CRC state
        switch (filler.state) {
        case KeysFillerStateSecurity:
            filler.state = KeysFillerStateSSID;
            break;
        case KeysFillerStateSSID:
            data->ssid[ filler.index ] = 0;
            filler.state = KeysFillerStatePassword;
            break;
        case KeysFillerStatePassword:
            data->password[ filler.index ] = 0;
            filler.state = KeysFillerStateToken;
            break;
        case KeysFillerStateToken:
            data2.device_key[ filler.index ] = 0;
            filler.state = KeysFillerStateCRC;
            break;
        case KeysFillerStateCRC:
            // wait
            break;
        default:
            break;
        }
        is_first_byte = 1;
        filler.index  = 0;
        return 0;
    }
    if ( ! (('0' <= code) && (code <= '9')) &&
         ! (('A' <= code) && (code <= 'F')) ) {
        // we only use letters which match: [0-9A-F,]
        Serial.print(P("unexpected code: 0x")); Serial.println( code, HEX );
        return -1;
    }
    if (filler.state == KeysFillerStateSecurity) {
        switch (code) {
        case '0': // GSwifi::GSSECURITY_NONE:
        case '1': // GSwifi::GSSECURITY_OPEN:
        case '2': // GSwifi::GSSECURITY_WEP:
        case '4': // GSwifi::GSSECURITY_WPA_PSK:
        case '8': // GSwifi::GSSECURITY_WPA2_PSK:
            data->security = (GSwifi::GSSECURITY)x2i(code);
            return 0;
        default:
            Serial.print(P("unexpected security: 0x")); Serial.println( code, HEX );
            return -1;
        }
    }
    // we transfer [0..9A..F] as ASCII
    // so 2 bytes construct 1 character, network *bit* order
    if (is_first_byte) {
        character          = x2i(code);
        character        <<= 4;         // F0h
        is_first_byte   = false;
        return 0;
    }
    else {
        character        += x2i(code); // 0Fh
        is_first_byte  = true;
    }

    switch (filler.state) {
    case KeysFillerStateSSID:
        if ( filler.index == MAX_WIFI_SSID_LENGTH ) {
            Serial.println(P("overflow 1"));
            return -1;
        }
        data->ssid[ filler.index ++ ] = character;
        break;
    case KeysFillerStatePassword:
        if ( filler.index == MAX_WIFI_PASSWORD_LENGTH ) {
            Serial.println(P("overflow 2"));
            return -1;
        }
        data->password[ filler.index ++ ] = character;
        break;
    case KeysFillerStateToken:
        if (filler.index == MAX_DEVICE_KEY_LENGTH) {
            Serial.println(P("overflow 3"));
            return -1;
        }
        data2.device_key[ filler.index ++ ] = character;
        break;
    case KeysFillerStateCRC:
        if (filler.index > 0) {
            Serial.println(P("overflow 4"));
            return -1;
        }
        data->crc8 = character;
        filler.index ++;
        break;
    default:
        Serial.println(P("??"));
        return -1;
    }
    return 0;
}

int8_t Keys::putDone()
{
    if (filler.state != KeysFillerStateCRC) {
        Serial.println(P("state error"));
        return -1;
    }

    data->is_set = true;
    data2.is_set = true;

    if (isCRCOK()) {
        return 0;
    }
    else {
        Serial.println(P("crc error"));
        return -1;
    }
}

void Keys::dump(void)
{
    Serial.print(P("wifi is_set: "));
    Serial.println(data->is_set);
    Serial.print(P("key is_set: "));
    Serial.println(data2.is_set);

    Serial.print(P("security: "));
    switch (data->security) {
    case GSwifi::GSSECURITY_AUTO:
        Serial.println(P("auto/none"));
        break;
    case GSwifi::GSSECURITY_OPEN:
        Serial.println(P("open"));
        break;
    case GSwifi::GSSECURITY_WEP:
        Serial.println(P("wep"));
        break;
    case GSwifi::GSSECURITY_WPA_PSK:
        Serial.println(P("wpa-psk"));
        break;
    case GSwifi::GSSECURITY_WPA2_PSK:
        Serial.println(P("wpa2-psk"));
        break;
    default:
        break;
    }

    Serial.print(P("ssid: "));
    Serial.println((const char*)data->ssid);

    Serial.print(P("password: "));
    Serial.println((const char*)data->password);

    Serial.print(P("device_key: "));
    Serial.println((const char*)data2.device_key);

    Serial.print(P("crc8: 0x"));
    Serial.println(data->crc8, HEX);
}