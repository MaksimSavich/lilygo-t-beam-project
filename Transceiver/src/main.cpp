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

// struct Command {
//     uint8_t arbID;   // First byte is Arb ID
//     uint8_t payloadSize;  // Second byte is Payload Size
//     String payload;  // Extract payload
//     volatile bool packetReady;
//     String receivedPacket;
// };

// enum NodeState {
//     STANDBY,
//     SETTINGS_UPDATE,
//     TESTING
// };

// NodeState currentState = STANDBY;  // Initial state
// Command command;

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
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
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

    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }
#endif

    setFlag();
}

// void handleSettingsUpdate() {
//         Serial.println("Applying new settings...");

//         loadSettingsFromFile(settings);
//         configureLoRaSettings(settings, radio); // Apply the new settings to the LoRa module
//         newSettingsReceived = false;           // Reset the flag

//         Serial.println("Settings updated.");
//         delay(1000);
//         readSettings();

//         currentState = STANDBY;  // Return to standby
// }

// void handleTesting() {
//     Serial.println("Running test operation...");

//     if (Serial.available()) {
//         String command = Serial.readStringUntil('\n');
//         if (command == "STOP_TEST") {
//             Serial.println("Stopping test operation...");
//             currentState = STANDBY;
//             return;
//         }
//     }

//     // Transmission or reception logic
//     int state = radio.startTransmit(testData, testDataSize);
//     if (state != RADIOLIB_ERR_NONE) {
//         Serial.println("Transmission error!");
//     }

//     delay(testInterval);  // User-defined interval between transmissions
// }

// void processPacket(const String& packet) {
//     if (packet.length() < 3) {  // Minimum size for Arb ID + End Signal
//         Serial.println("Invalid packet received");
//         return;
//     }

//     command.arbID = packet[0];   // First byte is Arb ID
//     command.payloadSize = packet[1];  // Second byte is Payload Size
//     command.payload = packet.substring(2, 2 + payloadSize);  // Extract payload

//     // Handle the packet based on Arb ID
//     switch (command.arbID) {
//         case 0x01:  // Start testing
//             Serial.println("Start testing command received.");
//             currentState = TESTING;
//             break;

//         case 0x02:  // Testing ongoing
//             Serial.println("Testing ongoing data received.");
//             break;

//         case 0x03:  // Stop testing
//             Serial.println("Stop testing command received.");
//             currentState = STANDBY;
//             break;

//         case 0x04:  // Settings update
//             Serial.println("Settings update command received.");
//             currentState = SETTINGS_UPDATE;
//             break;

//         default:
//             Serial.println("Unknown Arb ID received.");
//             break;
//     }
// }



void loop()
{
    // if (command.packetReady) {
    //     processPacket(command.receivedPacket);
    // }
    // switch (currentState) {
    //     case STANDBY:
    //         break;

    //     case SETTINGS_UPDATE:
    //         handleSettingsUpdate(command.payload);
    //         break;

    //     case TESTING:
    //         handleTesting();
    //         break;

    //     default:
    //         Serial.println("Unknown state.");
    //         currentState = STANDBY;
    //         break;
    // }
}

void serialEvent() {
    // while (Serial.available() && command.packetReady = true) {
    //     char c = Serial.read();
    //     command.receivedPacket += c;

    //     // Check for end signal (0xFF)
    //     if (c == '\xFF') {
    //         command.packetReady = true;
    //         break;
    //     }
    // }

        if (Serial.available()) {
        // Read data into a buffer
        uint8_t buffer[Settings_size];
        size_t len = Serial.readBytes(buffer, sizeof(buffer));

        // Deserialize the buffer into a Settings struct
        pb_istream_t stream = pb_istream_from_buffer(buffer, len);
        Settings receivedSettings = Settings_init_zero;

        if (!pb_decode(&stream, &Settings_msg, &receivedSettings)) {
            Serial.print("Failed to decode settings: ");
            Serial.println(PB_GET_ERROR(&stream));
            return;
        }

        // Optionally save the settings
        saveSettingsToFile(receivedSettings);
        configureLoRaSettings(settings, radio);
        readSettings();

        Serial.println("Settings updated successfully");
    }
}