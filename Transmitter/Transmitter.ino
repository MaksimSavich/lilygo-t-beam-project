/*
 * @file        boards.cpp
 * @authors     Lewis He (lewishe@outlook.com), Maksim Savich
 * @license     MIT
 * @copyright   Copyright (c) 2024  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date        NA
 * @last-update 2024-12-28
 */

#include "LoRaBoards.h"
#include <RadioLib.h>

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// Vars
DataPacket packet;
TinyGPSPlus gps;
static int transmissionState = RADIOLIB_ERR_NONE; // save transmission state between loops
static volatile bool transmittedFlag = false;     // flag to indicate that a packet was sent
volatile bool newSettingsReceived = false;        // Flag for new settings
Settings settings;
Settings updatedSettings; 

// Data packet struct
struct DataPacket {
    uint16_t packet_id = 1;
    double latitude;           // 8 bytes: Latitude
    double longitude;          // 8 bytes: Longitude
    uint8_t num_satellites;    // 1 byte: Number of satellites
};

// Settings struct
struct Settings {
    float frequency;          // 4 bytes
    int power;                // 4 bytes
    float bandwidth;          // 4 bytes
    int spreading_factor;     // 4 bytes
    int coding_rate;          // 4 bytes
    int preamble;             // 4 bytes
    bool set_crc;             // 1 byte
    uint8_t sync_word;        // 1 byte
};  

void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void saveSettingsToFile(const Settings& settings) {
    File file = LittleFS.open("/settings.json", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open /settings.json for writing");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["frequency"] = settings.frequency;
    doc["power"] = settings.power;
    doc["bandwidth"] = settings.bandwidth;
    doc["spreading_factor"] = settings.spreading_factor;
    doc["coding_rate"] = settings.coding_rate;
    doc["preamble"] = settings.preamble;
    doc["set_crc"] = settings.set_crc;
    doc["sync_word"] = settings.sync_word;

    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write JSON to file");
    }
    file.close();
}

void loadSettingsFromFile(Settings& settings) {
    File file = LittleFS.open("/settings.json", FILE_READ);
    if (!file) {
        Serial.println("Failed to open /settings.json for reading");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);

    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        file.close();
        return;
    }

    settings.frequency = doc["frequency"];
    settings.power = doc["power"];
    settings.bandwidth = doc["bandwidth"];
    settings.spreading_factor = doc["spreading_factor"];
    settings.coding_rate = doc["coding_rate"];
    settings.preamble = doc["preamble"];
    settings.set_crc = doc["set_crc"];
    settings.sync_word = doc["sync_word"];
    file.close();
}

void configureLoRa(const Settings& settings, SX1262& radio) {
    if (radio.setFrequency(settings.frequency) != RADIOLIB_ERR_INVALID_FREQUENCY) Serial.println("Selected frequency is invalid for this module!");
    if (radio.setOutputPower(settings.power) != RADIOLIB_ERR_INVALID_OUTPUT_POWER) Serial.println("Selected output power is invalid for this module!");
    if (radio.setBandwidth(settings.bandwidth) != RADIOLIB_ERR_INVALID_BANDWIDTH) Serial.println("Selected bandwidth is invalid for this module!");
    if (radio.setSpreadingFactor(settings.spreading_factor) != RADIOLIB_ERR_INVALID_SPREADING_FACTOR) Serial.println("Selected spreading factor is invalid for this module!");
    if (radio.setCodingRate(settings.coding_rate) != RADIOLIB_ERR_INVALID_CODING_RATE) Serial.println("Selected coding rate is invalid for this module!");
    if (radio.setPreambleLength(settings.preamble) != RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) Serial.println("Selected preamble length is invalid for this module!");
    if (radio.setCRC(settings.set_crc) != RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) Serial.println("Selected CRC is invalid for this module!");
    if (radio.setSyncWord(settings.sync_word) != RADIOLIB_ERR_NONE) Serial.println("Unable to set sync word!");
}

void initializeSettings(Settings& settings, SX1262& radio) {
    File file = LittleFS.open("/settings.json", FILE_READ);

    if (!file) {
        // If the file doesn't exist, create it with default settings
        Serial.println("Settings file not found. Creating with default values...");
        Settings defaultSettings = {
            915.0,   // frequency
            22,      // power
            500.0,   // bandwidth
            7,       // spreading factor
            5,       // coding rate
            8,       // preamble
            true,    // set crc
            0xAB     // sync word
        };

        // Save the default settings to a new file
        saveSettingsToFile(defaultSettings);

        settings = defaultSettings; // Load the default settings into the provided settings reference
    } else {
        // If the file exists, load the settings
        Serial.println("Settings file found. Loading values...");
        loadSettingsFromFile(settings);
        file.close();
    }
    configureLoRa(settings, radio); // Apply the settings to the LoRa radio module
}

void readFile(const char* path) {
    // Attempt to open the file in read mode
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        Serial.print("Failed to open file: ");
        Serial.println(path);
        return;
    }

    Serial.print("Contents of ");
    Serial.print(path);
    Serial.println(":");

    // Read the file line by line or character by character
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();  // Print a new line after file content

    file.close();  // Always close the file when done
}

void setup()
{
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // Format LittleFS if mounting fails
        Serial.println("Failed to initialize LittleFS");
        while (true);
    }
    Serial.println("LittleFS initialized successfully");
  
    delay(500);

    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

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
    readFile("/settings.json");

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

    setflag();
}

void loop()
{

  if (newSettingsReceived) {
        Serial.println("Applying new settings...");

        configureLoRa(updatedSettings, radio); // Apply the new settings to the LoRa module
        saveSettingsToFile(updatedSettings);   // Save the settings to LittleFS for persistence
        newSettingsReceived = false;           // Reset the flag

        Serial.println("Settings updated.");
        readFile("/settings.json");
    }

    // check if the previous transmission finished
    if (transmittedFlag) {
        transmittedFlag = false; // reset flag

        flashLed();

        // Temporary byte array for testing
        uint8_t byteArr[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE
      };

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

        // Send the packet via stream of bytes over serial
        // Experimenting | May be changed later to a better method
        Serial.write(0xAA);                               // Start byte
        Serial.write((uint8_t*)&packet, sizeof(packet));  // Send struct as bytes
        Serial.write(0x55);                               // End byte

        packet.packet_id++;                               // Increment packet ID | For testing

        delay(200);                                       // Wait ~200 ms before sending the next packet

    }
}

void serialEvent() {
    if (Serial.available()) {
        String incomingData = Serial.readStringUntil('\n');  // Read incoming JSON string until new line
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, incomingData);

        if (!error) {
            // Parse the JSON into the updated settings structure
            updatedSettings.frequency = doc["frequency"];
            updatedSettings.power = doc["power"];
            updatedSettings.bandwidth = doc["bandwidth"];
            updatedSettings.spreading_factor = doc["spreading_factor"];
            updatedSettings.coding_rate = doc["coding_rate"];
            updatedSettings.preamble = doc["preamble"];
            updatedSettings.set_crc = doc["set_crc"];
            updatedSettings.sync_word = doc["sync_word"];

            // Set the flag to indicate new settings are available
            newSettingsReceived = true;
            Serial.println("New settings received.");
        } else {
            Serial.println("Error parsing settings update JSON.");
        }
    }
}