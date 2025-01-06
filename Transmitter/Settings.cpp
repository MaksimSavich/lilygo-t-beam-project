#include "Settings.h"

void initializeSettings(Settings& settings, SX1262& radio) {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to initialize LittleFS");
        while (true);
    }
    Serial.println("LittleFS initialized successfully");

    File file = LittleFS.open(CONFIG_RADIO_SETTINGS_FILE, FILE_READ);

    if (!file) {
        // If the file doesn't exist, create it with default settings
        Serial.println("Settings file not found. Creating with default values...");

        settings.frequency = CONFIG_RADIO_FREQ;
        settings.power = CONFIG_RADIO_OUTPUT_POWER;
        settings.bandwidth = CONFIG_RADIO_BW;
        settings.spreading_factor = CONFIG_RADIO_SF;
        settings.coding_rate = CONFIG_RADIO_CR;
        settings.preamble = CONFIG_RADIO_PREAMBLE;
        settings.set_crc = CONFIG_RADIO_SETCRC;
        settings.sync_word = CONFIG_RADIO_SW;

        saveSettingsToFile(settings);
    } else {
        Serial.println("Settings file found. Loading values...");
        file.close();
    }

    loadSettingsFromFile(settings);
    configureLoRaSettings(settings, radio);
}

void loadSettingsFromFile(Settings& settings) {
    File file = LittleFS.open(CONFIG_RADIO_SETTINGS_FILE, FILE_READ);
    if (!file) {
        Serial.println("Failed to open settings file for reading");
        return;
    }

    // Read binary data
    uint8_t buffer[file.size()];
    file.read(buffer, file.size());
    file.close();

    // Deserialize Protobuf data
    pb_istream_t stream = pb_istream_from_buffer(buffer, sizeof(buffer));
    if (!pb_decode(&stream, &Settings_msg, &settings)) {
        Serial.println("Failed to decode settings");
        return;
    }
}

void saveSettingsToFile(const Settings& settings) {
    File file = LittleFS.open(CONFIG_RADIO_SETTINGS_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open settings file for writing");
        return;
    }

    // Serialize Protobuf data
    uint8_t buffer[Settings_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, &Settings_msg, &settings)) {
        Serial.println("Failed to encode settings");
        file.close();
        return;
    }

    file.write(buffer, stream.bytes_written);
    file.close();
    Serial.println("Settings successfully written");
}

void readSettings() {
    File file = LittleFS.open(CONFIG_RADIO_SETTINGS_FILE, FILE_READ);
    if (!file) {
        Serial.println("Failed to open settings file for reading");
        return;
    }

    uint8_t buffer[file.size()];
    file.read(buffer, file.size());
    file.close();

    // Deserialize Protobuf data
    Settings settings = Settings_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buffer, sizeof(buffer));
    if (!pb_decode(&stream, &Settings_msg, &settings)) {
        Serial.println("Failed to decode settings");
        return;
    }

    // Print settings values
    Serial.println("Settings file contents:");
    Serial.print("Frequency: ");
    Serial.println(settings.frequency);
    Serial.print("Power: ");
    Serial.println(settings.power);
    Serial.print("Bandwidth: ");
    Serial.println(settings.bandwidth);
    Serial.print("Spreading Factor: ");
    Serial.println(settings.spreading_factor);
    Serial.print("Coding Rate: ");
    Serial.println(settings.coding_rate);
    Serial.print("Preamble: ");
    Serial.println(settings.preamble);
    Serial.print("Set CRC: ");
    Serial.println(settings.set_crc ? "True" : "False");
    Serial.print("Sync Word: ");
    Serial.println(settings.sync_word);
}

void configureLoRaSettings(const Settings& settings, SX1262& radio) {
    if (radio.setFrequency(settings.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY)
        Serial.println("Error: Selected frequency is invalid for this module!");
    if (radio.setOutputPower(settings.power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
        Serial.println("Error: Selected output power is invalid for this module!");
    if (radio.setBandwidth(settings.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH)
        Serial.println("Error: Selected bandwidth is invalid for this module!");
    if (radio.setSpreadingFactor(settings.spreading_factor) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
        Serial.println("Error: Selected spreading factor is invalid for this module!");
    if (radio.setCodingRate(settings.coding_rate) == RADIOLIB_ERR_INVALID_CODING_RATE)
        Serial.println("Error: Selected coding rate is invalid for this module!");
    if (radio.setPreambleLength(settings.preamble) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
        Serial.println("Error: Selected preamble length is invalid for this module!");
    if (radio.setCRC(settings.set_crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
        Serial.println("Error: Selected CRC is invalid for this module!");
    if (radio.setSyncWord(settings.sync_word) != RADIOLIB_ERR_NONE)
        Serial.println("Error: Unable to set sync word!");
}