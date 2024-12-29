#include "Settings.h"

void initializeSettings(Settings& settings, SX1262& radio) {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // Format LittleFS if mounting fails
        Serial.println("Failed to initialize LittleFS");
        while (true);
    }
    Serial.println("LittleFS initialized successfully");

    File file = LittleFS.open("/settings.json", FILE_READ);

    if (!file) {
        // If the file doesn't exist, create it with default settings
        Serial.println("Settings file not found. Creating with default values...");

        StaticJsonDocument<256> doc;
        doc["frequency"] = CONFIG_RADIO_FREQ;
        doc["power"] = CONFIG_RADIO_OUTPUT_POWER;
        doc["bandwidth"] = CONFIG_RADIO_BW;
        doc["spreading_factor"] = CONFIG_RADIO_SF;
        doc["coding_rate"] = CONFIG_RADIO_CR;
        doc["preamble"] = CONFIG_RADIO_PREAMBLE;
        doc["set_crc"] = CONFIG_RADIO_SETCRC;
        doc["sync_word"] = CONFIG_RADIO_SW;

        // Save the default settings to a new file
        saveSettingsToFile(doc);
        
    } else {
        // If the file exists, load the settings
        Serial.println("Settings file found. Loading values...");
        file.close();
    }
    loadSettingsFromFile(settings);
    configureLoRaSettings(settings, radio); // Apply the settings to the LoRa radio module
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

void saveSettingsToFile(const StaticJsonDocument<256>& doc) {
    File file = LittleFS.open("/settings.json", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open /settings.json for writing");
        return;
    }

    // Serialize the JSON document to the file
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write JSON to file");
    } else {
        Serial.println("Settings successfully written to /settings.json");
    }
    file.close();
}

void readSettings() {
    // Attempt to open the file in read mode
    File file = LittleFS.open(CONFIG_RADIO_SETTINGS_FILE, FILE_READ);
    if (!file) {
        Serial.print("Failed to open file: ");
        Serial.println(CONFIG_RADIO_SETTINGS_FILE);
        return;
    }

    Serial.print("Contents of ");
    Serial.print(CONFIG_RADIO_SETTINGS_FILE);
    Serial.println(":");

    // Read the file line by line or character by character
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();  // Print a new line after file content

    file.close();  // Always close the file when done
}

void configureLoRaSettings(const Settings& settings, SX1262& radio) {
    if (radio.setFrequency(settings.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY) Serial.println("Error: Selected frequency is invalid for this module!");
    if (radio.setOutputPower(settings.power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) Serial.println("Error: Selected output power is invalid for this module!");
    if (radio.setBandwidth(settings.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH) Serial.println("Error: Selected bandwidth is invalid for this module!");
    if (radio.setSpreadingFactor(settings.spreading_factor) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) Serial.println("Error: Selected spreading factor is invalid for this module!");
    if (radio.setCodingRate(settings.coding_rate) == RADIOLIB_ERR_INVALID_CODING_RATE) Serial.println("Error: Selected coding rate is invalid for this module!");
    if (radio.setPreambleLength(settings.preamble) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) Serial.println("Error: Selected preamble length is invalid for this module!");
    if (radio.setCRC(settings.set_crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) Serial.println("Error: Selected CRC is invalid for this module!");
    if (radio.setSyncWord(settings.sync_word) != RADIOLIB_ERR_NONE) Serial.println("Error: Unable to set sync word!");
}