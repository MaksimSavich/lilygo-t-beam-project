/*
 * @file        boards.cpp
 * @authors     Lewis He (lewishe@outlook.com), Maksim Savich
 * @license     MIT
 * @copyright   Copyright (c) 2024  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date        NA
 * @last-update 2024-12-28
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <TinyGPS++.h>

#include "LoRaBoards.h"
#include "Settings.h"

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// // Data packet struct
// struct DataPacket {
//     uint16_t packet_id = 1;
//     double latitude;           // 8 bytes: Latitude
//     double longitude;          // 8 bytes: Longitude
//     uint8_t num_satellites;    // 1 byte: Number of satellites
// };

enum NodeState
{
    STANDBY,
    TESTING
};

NodeState currentState = STANDBY; // Initial state

// Vars
// DataPacket packet;
WiFiServer server(80);
WiFiClient client;
TinyGPSPlus gps;
static int transmissionState = RADIOLIB_ERR_NONE; // save transmission state between loops
static volatile bool transmittedFlag = false;     // flag to indicate that a packet was sent
volatile bool newSettingsReceived = false;        // Flag for new settings
Settings settings;

void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void setup()
{
    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

    server.begin();

    // initialize radio with default settings
    int state = radio.begin();

    printResult(state == RADIOLIB_ERR_NONE);

    Serial.print(F("Radio Initializing ... "));
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    radio.setPacketSentAction(setFlag); // set the function that will be called when packet transmission is finished

    initializeSettings(settings, radio);
    readSettings();

#if !defined(USING_SX1280) && !defined(USING_LR1121) && !defined(USING_SX1280PA)
    /*
     * Sets current limit for over current protection at transmitter amplifier.
     * SX1262/SX1268 : Allowed values range from 45 to 120 mA in 2.5 mA steps and 120 to 240 mA in 10 mA steps.
     * NOTE: set value to 0 to disable overcurrent protection
     */

    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true)
            ;
    }
#endif

    setFlag();
}

void loop()
{
    while (true)
    {

        // Allocate buffer for the full packet
        uint8_t fullBuffer[Packet_size] = {0};
        size_t fullLen = Serial.readBytes(fullBuffer, Packet_size);

        pb_istream_t fullStream = pb_istream_from_buffer(fullBuffer, fullLen);
        Packet receivedPacket = Packet_init_zero;

        // Decode the full packet
        if (!pb_decode(&fullStream, &Packet_msg, &receivedPacket))
        {
            Serial.print("Failed to decode full packet: ");
            Serial.println(PB_GET_ERROR(&fullStream));
            continue;
        }
        // Process the packet based on its type
        if (receivedPacket.type == PacketType_SETTINGS)
        {
            Serial.println("Settings packet received.");
            saveSettingsToFile(receivedPacket.settings);
            configureLoRaSettings(receivedPacket.settings, radio);
            Serial.println("Settings updated successfully.");
        }
        else if (receivedPacket.type == PacketType_TRANSMISSION)
        {
            Serial.println("Transmission packet received.");

            if (receivedPacket.transmission.payload.bytes > 0)
            {
                int state = radio.transmit(receivedPacket.transmission.payload.bytes, sizeof(receivedPacket.transmission.payload.bytes));
                if (state == RADIOLIB_ERR_NONE)
                {
                    flashLed();
                    Serial.println("Packet transmitted successfully.");
                }
                else
                {
                    Serial.println("Transmission error.");
                }
            }
            else
            {
                Serial.println("Empty payload, transmission skipped.");
            }
        }
        else
        {
            Serial.println("Unknown packet type.");
        }
    }
}