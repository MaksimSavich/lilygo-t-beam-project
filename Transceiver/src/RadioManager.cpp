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

    radio.setPacketSentAction(transmittedISR);
    radio.setPacketReceivedAction(receivedISR);
    configure(settings);
    startReceive();

    return true;
}

bool RadioManager::configure(const SettingsManager &settings)
{
    if (radio.setFrequency(settings.config.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY)
    {
        Serial.println("Error: Selected frequency is invalid for this module!");
    }

    if (radio.setOutputPower(settings.config.power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
    {
        Serial.println("Error: Selected output power is invalid for this module!");
    }

    if (radio.setBandwidth(settings.config.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH)
    {
        Serial.println("Error: Selected bandwidth is invalid for this module!");
    }

    if (radio.setSpreadingFactor(settings.config.spreading_factor) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
    {
        Serial.println("Error: Selected spreading factor is invalid for this module!");
    }

    if (radio.setCodingRate(settings.config.coding_rate) == RADIOLIB_ERR_INVALID_CODING_RATE)
    {
        Serial.println("Error: Selected coding rate is invalid for this module!");
    }

    if (radio.setPreambleLength(settings.config.preamble) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
    {
        Serial.println("Error: Selected preamble length is invalid for this module!");
    }

    if (radio.setCRC(settings.config.set_crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
    {
        Serial.println("Error: Selected CRC is invalid for this module!");
    }

    if (radio.setSyncWord(settings.config.sync_word) != RADIOLIB_ERR_NONE)
    {
        Serial.println("Error: Unable to set sync word!");
    }
    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        Serial.println(F("Selected current limit is invalid for this module!"));
    }

    return true;
}

void RadioManager::transmit(const uint8_t *data, size_t length)
{
    if (transmittedFlag)
    {
        transmittedFlag = false;

        int state = radio.transmit(data, length);
    }
}

void RadioManager::startReceive()
{
    radio.startReceive();
}

void RadioManager::processReceivedPacket()
{
    if (receivedFlag)
    {
        receivedFlag = false;
        Reception reception = Reception_init_zero;

        int state = radio.readData(reception.payload.bytes, 255);
        reception.payload.size = 255;

        processGPSData(reception);

        reception.rssi = radio.getRSSI();
        reception.snr = radio.getSNR();
        reception.crc_error = (state == RADIOLIB_ERR_CRC_MISMATCH);
        reception.general_error = (state != RADIOLIB_ERR_NONE && !reception.crc_error);

        sendReceptionProto(reception);
        startReceive();
    }
}

void RadioManager::processGPSData(Reception &reception)
{
    while (gpsSerial.available())
    {
        if (gps.encode(gpsSerial.read()))
        {
            reception.latitude = gps.location.isValid() ? gps.location.lat() : 0;
            reception.longitude = gps.location.isValid() ? gps.location.lng() : 0;
            reception.sattelites = gps.satellites.isValid() ? gps.satellites.value() : 0;
        }
    }
}

void RadioManager::sendReceptionProto(const Reception &reception)
{
    uint8_t buffer[512];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Reception_fields, &reception))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}