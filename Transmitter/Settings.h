#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <RadioLib.h>

#define CONFIG_RADIO_FREQ           915.0
#define CONFIG_RADIO_OUTPUT_POWER   22
#define CONFIG_RADIO_BW             500.0
#define CONFIG_RADIO_SF             8
#define CONFIG_RADIO_CR             5
#define CONFIG_RADIO_PREAMBLE       8
#define CONFIG_RADIO_SETCRC         true
#define CONFIG_RADIO_SW             0xAB
#define CONFIG_RADIO_SETTINGS_FILE  "/settings.json"

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

void initializeSettings(Settings& settings, SX1262& radio);
void loadSettingsFromFile(Settings& settings);
void saveSettingsToFile(const StaticJsonDocument<256>& doc);
void configureLoRaSettings(const Settings& settings, SX1262& radio);
void readSettings();