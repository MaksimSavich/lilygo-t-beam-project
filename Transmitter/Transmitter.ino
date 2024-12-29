/*
 * @file        boards.cpp
 * @authors     Lewis He (lewishe@outlook.com), Maksim Savich
 * @license     MIT
 * @copyright   Copyright (c) 2024  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date        NA
 * @last-update 2024-12-28
 */

#include <RadioLib.h>
#include <TinyGPS++.h>
#include "LoRaBoards.h"
#include "Settings.h"

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// Data packet struct
struct DataPacket {
    uint16_t packet_id = 1;
    double latitude;           // 8 bytes: Latitude
    double longitude;          // 8 bytes: Longitude
    uint8_t num_satellites;    // 1 byte: Number of satellites
};

enum NodeState {
    STANDBY,
    SETTINGS_UPDATE,
    TESTING
};

NodeState currentState = STANDBY;  // Initial state

// Vars
DataPacket packet;
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

void loop()
{

  if (newSettingsReceived) {
        Serial.println("Applying new settings...");

        loadSettingsFromFile(settings);
        configureLoRaSettings(settings, radio); // Apply the new settings to the LoRa module
        newSettingsReceived = false;           // Reset the flag

        Serial.println("Settings updated.");
        delay(1000);
        readSettings();
    }

    // check if the previous transmission finished
    if (transmittedFlag) {
        transmittedFlag = false; // reset flag

        flashLed();

      int state = radio.startTransmit(byteArr, 255);

      while (SerialGPS.available()) {
          if (gps.encode(SerialGPS.read())) {
              if (gps.location.isValid()) {
                packet.latitude = gps.location.lat();
                packet.longitude = gps.location.lng();
                packet.num_satellites = gps.satellites.value();
              }
              else{
                packet.latitude = NULL;
                packet.longitude = NULL;
                packet.num_satellites = NULL;
              }
          }
      }



    }
}

void serialEvent() {
    if (Serial.available()) {
      if(true){ // Placeholder for settings update JSON detection logic
        String incomingData = Serial.readStringUntil('\n');  // Read incoming JSON string until new line
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, incomingData);

        if (!error) {
            saveSettingsToFile(doc); // Save incoming settings JSON doc
            newSettingsReceived = true;// Set the flag to indicate new settings are available
            Serial.println("New settings received.");
        } else {
            Serial.println("Error parsing settings update JSON.");
        }
      }
    }
}