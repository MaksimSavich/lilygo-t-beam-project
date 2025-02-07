#include "RadioManager.h"
#include <pb_encode.h>

RadioManager *RadioManager::instance = nullptr;

RadioManager::RadioManager(SX1262 &radio, HardwareSerial &gpsSerial)
    : radio(radio), gpsSerial(gpsSerial), transmittedFlag(false), receivedFlag(false)
{
    instance = this;
}

bool RadioManager::initialize(SettingsManager &settings)
{
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE)
    {
        return false;
    }
    configure(settings);

    // Since I don't send an initial message at startup, transmitted must be set true at init
    transmittedFlag = true;

    return true;
}

bool RadioManager::configure(const SettingsManager &settings)
{

    if (radio.setFrequency(settings.config.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY)
    {
        Serial.println("Error: Selected frequency is invalid for this module!");
        return false;
    }

    if (radio.setOutputPower(settings.config.power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
    {
        Serial.println("Error: Selected output power is invalid for this module!");
        return false;
    }

    if (radio.setBandwidth(settings.config.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH)
    {
        Serial.println("Error: Selected bandwidth is invalid for this module!");
        return false;
    }

    if (radio.setSpreadingFactor(settings.config.spreading_factor) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
    {
        Serial.println("Error: Selected spreading factor is invalid for this module!");
        return false;
    }

    if (radio.setCodingRate(settings.config.coding_rate) == RADIOLIB_ERR_INVALID_CODING_RATE)
    {
        Serial.println("Error: Selected coding rate is invalid for this module!");
    }

    if (radio.setPreambleLength(settings.config.preamble) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
    {
        Serial.println("Error: Selected preamble length is invalid for this module!");
        return false;
    }

    if (radio.setCRC(settings.config.set_crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
    {
        Serial.println("Error: Selected CRC is invalid for this module!");
        return false;
    }

    if (radio.setSyncWord(settings.config.sync_word) != RADIOLIB_ERR_NONE)
    {
        Serial.println("Error: Unable to set sync word!");
        return false;
    }
    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        Serial.println(F("Selected current limit is invalid for this module!"));
        return false;
    }

    return true;
}

void RadioManager::transmit(const uint8_t *data, size_t length)
{
    if (transmittedFlag)
    {
        transmittedFlag = false;
        int state = radio.startTransmit(data, length);
        processTransmitLog(state);
        flashLed();
    }
}

void RadioManager::startReceive()
{
    radio.startReceive();
    while (!receivedFlag)
    {
        int currentRssi = radio.getRSSI(false);

        // Ensure FIFO behavior: remove oldest if max size is reached
        if (instantRssiCollection.size() >= 100)
        {
            instantRssiCollection.pop_front();
        }

        instantRssiCollection.push_back(currentRssi);
    }
}

void RadioManager::processReceptionLog()
{
    if (receivedFlag)
    {
        flashLed();
        receivedFlag = false;
        Log log = Log_init_zero;

        size_t loraPacketLength = radio.getPacketLength();
        int state = radio.readData(log.payload.bytes, loraPacketLength);
        log.payload.size = loraPacketLength;

        log.has_gps = true;
        ProcessGPSData(log.gps);

        // Fill RSSI collection using bytes (400 bytes max)
        size_t rssiCount = std::min(instantRssiCollection.size(), static_cast<size_t>(400));
        log.rssiCollection.size = rssiCount * sizeof(int32_t); // Store actual byte size
        // Copy RSSI values as raw int32_t values (ensures proper alignment)
        for (size_t i = 0; i < rssiCount; ++i)
        {
            int32_t rssi = instantRssiCollection[i];
            memcpy(&log.rssiCollection.bytes[i * sizeof(int32_t)], &rssi, sizeof(int32_t));
        }
        instantRssiCollection.clear();

        log.snr = radio.getSNR();
        log.crc_error = (state == RADIOLIB_ERR_CRC_MISMATCH);
        log.general_error = (state != RADIOLIB_ERR_NONE && !log.crc_error);

        TxSerialLogPacket(log);
        startReceive();
    }
}

void RadioManager::processTransmitLog(int state)
{
    Log log = Log_init_zero;

    log.has_gps = true;
    ProcessGPSData(log.gps);
    log.snr = 0;
    log.crc_error = false;
    log.general_error = (state != RADIOLIB_ERR_NONE);

    TxSerialLogPacket(log);
}

void RadioManager::ProcessGPSData(Gps &gpsData)
{
    while (gpsSerial.available())
    {
        if (gps.encode(gpsSerial.read()))
        {
            gpsData.latitude = gps.location.isValid() ? gps.location.lat() : 0;
            gpsData.longitude = gps.location.isValid() ? gps.location.lng() : 0;
            gpsData.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
        }
    }
}

void RadioManager::TxSerialLogPacket(const Log &log)
{
    Packet packet = Packet_init_zero;
    packet.has_log = true;
    packet.type = PacketType_LOG;
    packet.log = log;

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

void RadioManager::TxSerialGPSPacket()
{
    Packet packet = Packet_init_zero;
    packet.has_gps = true;
    packet.type = PacketType_GPS;

    ProcessGPSData(packet.gps);

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

void RadioManager::setState(State newState)
{
    if (newState == State_RECEIVER)
    {
        radio.setPacketReceivedAction(receivedISR);
        startReceive();
    }
    else if (newState == State_TRANSMITTER)
    {
        radio.setPacketSentAction(transmittedISR);
    }

    state = newState;
}
