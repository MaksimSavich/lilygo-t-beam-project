#include "SettingsManager.h"
#include <stdexcept>

SettingsManager::SettingsManager(SX1262 &radio) : radio(radio)
{
    config = Settings_init_zero;
}

bool SettingsManager::initialize()
{
    initFilesystem();

    if (!LittleFS.exists(filename))
    {
        createDefaults();
        save();
    }

    load();
    return validate();
}

void SettingsManager::initFilesystem()
{
    if (!LittleFS.begin(true))
    {
        throw std::runtime_error("LittleFS init failed");
    }
}

void SettingsManager::load()
{
    File file = LittleFS.open(filename, FILE_READ);
    if (!file)
        throw std::runtime_error("Failed to open settings file");

    size_t size = file.size();
    uint8_t buffer[size];
    file.read(buffer, size);
    file.close();

    pb_istream_t stream = pb_istream_from_buffer(buffer, size);
    if (!pb_decode(&stream, Settings_fields, &config))
    {
        throw std::runtime_error("Protobuf decode failed");
    }
}

void SettingsManager::save()
{
    File file = LittleFS.open(filename, FILE_WRITE);
    if (!file)
        throw std::runtime_error("Failed to create settings file");

    uint8_t buffer[Settings_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, Settings_fields, &config))
    {
        file.close();
        throw std::runtime_error("Protobuf encode failed");
    }

    file.write(buffer, stream.bytes_written);
    file.close();
}

void SettingsManager::print() const
{
    Serial.println("Settings file contents:");
    Serial.print("Frequency: ");
    Serial.println(config.frequency);
    Serial.print("Power: ");
    Serial.println(config.power);
    Serial.print("Bandwidth: ");
    Serial.println(config.bandwidth);
    Serial.print("Spreading Factor: ");
    Serial.println(config.spreading_factor);
    Serial.print("Coding Rate: ");
    Serial.println(config.coding_rate);
    Serial.print("Preamble: ");
    Serial.println(config.preamble);
    Serial.print("Set CRC: ");
    Serial.println(config.set_crc ? "True" : "False");
    Serial.print("Sync Word: ");
    Serial.println(config.sync_word);
    Serial.print("Func State: ");
    Serial.println(config.func_state);
}

void SettingsManager::sendProto()
{
    Packet packet = Packet_init_zero;
    packet.type = PacketType_SETTINGS;
    packet.has_settings = true;
    packet.settings = config;

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

void SettingsManager::createDefaults()
{
    config = {
        .frequency = 915.0,
        .power = 22,
        .bandwidth = 500.0,
        .spreading_factor = 7,
        .coding_rate = 5,
        .preamble = 8,
        .set_crc = true,
        .sync_word = 0xAB,
        .func_state = FuncState_TRANSMITTER};
}

bool SettingsManager::validate() const
{
    return (config.frequency >= 400.0 && config.frequency <= 960.0) &&
           (config.power >= -3 && config.power <= 22) &&
           (config.spreading_factor >= 5 && config.spreading_factor <= 12);
}