/**
 * @file SettingsManager.cpp
 * @brief Manages the settings for the SX1262 radio module using LittleFS and Protocol Buffers.
 */

#include "SettingsManager.h"
#include <stdexcept>

/**
 * @brief Constructor for SettingsManager.
 * @param mRadio Reference to the SX1262 radio module.
 */
SettingsManager::SettingsManager(SX1262 &radio) : mRadio(radio)
{
    mConfig = Settings_init_zero;
}

/**
 * @brief Initializes the settings manager.
 * @return True if settings are valid, false otherwise.
 */
bool SettingsManager::initialize()
{
    initFilesystem();

    if (!LittleFS.exists(mFilename))
    {
        createDefaults();
        save();
    }

    load();
    return validate();
}

/**
 * @brief Initializes the LittleFS filesystem.
 * @throws std::runtime_error if filesystem initialization fails.
 */
void SettingsManager::initFilesystem()
{
    if (!LittleFS.begin(true))
    {
        throw std::runtime_error("LittleFS init failed");
    }
}

/**
 * @brief Loads the settings from the filesystem.
 * @throws std::runtime_error if the settings file cannot be opened or decoded.
 */
void SettingsManager::load()
{
    File file = LittleFS.open(mFilename, FILE_READ);
    if (!file)
        throw std::runtime_error("Failed to open settings file");

    size_t size = file.size();
    uint8_t buffer[size];
    file.read(buffer, size);
    file.close();

    pb_istream_t stream = pb_istream_from_buffer(buffer, size);
    if (!pb_decode(&stream, Settings_fields, &mConfig))
    {
        throw std::runtime_error("Protobuf decode failed");
    }
}

/**
 * @brief Saves the settings to the filesystem.
 * @throws std::runtime_error if the settings file cannot be created or encoded.
 */
void SettingsManager::save()
{
    File file = LittleFS.open(mFilename, FILE_WRITE);
    if (!file)
        throw std::runtime_error("Failed to create settings file");

    uint8_t buffer[Settings_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, Settings_fields, &mConfig))
    {
        file.close();
        throw std::runtime_error("Protobuf encode failed");
    }

    file.write(buffer, stream.bytes_written);
    file.close();
}

/**
 * @brief Prints the current settings to the serial output.
 */
void SettingsManager::print() const
{
    Serial.println("Settings file contents:");
    Serial.print("Frequency: ");
    Serial.println(mConfig.frequency);
    Serial.print("Power: ");
    Serial.println(mConfig.power);
    Serial.print("Bandwidth: ");
    Serial.println(mConfig.bandwidth);
    Serial.print("Spreading Factor: ");
    Serial.println(mConfig.spreading_factor);
    Serial.print("Coding Rate: ");
    Serial.println(mConfig.coding_rate);
    Serial.print("Preamble: ");
    Serial.println(mConfig.preamble);
    Serial.print("Set CRC: ");
    Serial.println(mConfig.set_crc ? "True" : "False");
    Serial.print("Sync Word: ");
    Serial.println(mConfig.sync_word);
}

/**
 * @brief Sends the current settings as a protobuf packet over the serial connection.
 */
void SettingsManager::sendProto()
{
    Packet packet = Packet_init_zero;
    packet.type = PacketType_SETTINGS;
    packet.has_settings = true;
    packet.settings = mConfig;

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

/**
 * @brief Creates default settings.
 */
void SettingsManager::createDefaults()
{
    mConfig = {
        .frequency = 915.0,
        .power = 22,
        .bandwidth = 500.0,
        .spreading_factor = 7,
        .coding_rate = 5,
        .preamble = 8,
        .set_crc = true,
        .sync_word = 0xAB,
    };
}

/**
 * @brief Validates the current settings.
 * @return True if settings are valid, false otherwise.
 */
bool SettingsManager::validate() const
{
    return (mConfig.frequency >= 400.0 && mConfig.frequency <= 960.0) &&
           (mConfig.power >= -3 && mConfig.power <= 22) &&
           (mConfig.spreading_factor >= 5 && mConfig.spreading_factor <= 12);
}