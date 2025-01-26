#pragma once
#include <RadioLib.h>
#include <TinyGPS++.h>
#include "SettingsManager.h"
#include "packet.pb.h"

class RadioManager
{
public:
    RadioManager(SX1262 &radio, HardwareSerial &gpsSerial);
    bool initialize(SettingsManager &settings);
    bool configure(const SettingsManager &settings);
    void transmit(const uint8_t *data, size_t length);
    void startReceive();
    void processReceivedPacket();
    void handleTransmitted() { transmittedFlag = true; }
    void handleReceived() { receivedFlag = true; }
    bool isTransmitted() const { return transmittedFlag; }
    bool isReceived() const { return receivedFlag; }

private:
    static constexpr const char *START_DELIMITER = "<START>";
    static constexpr const char *END_DELIMITER = "<END>";
    static constexpr size_t START_LEN = 7; // strlen("<START>")
    static constexpr size_t END_LEN = 5;   // strlen("<END>")

    static RadioManager *instance;
    static void transmittedISR()
    {
        if (instance)
            instance->handleTransmitted();
    }
    static void receivedISR()
    {
        if (instance)
            instance->handleReceived();
    }

    SX1262 &radio;
    HardwareSerial &gpsSerial;
    TinyGPSPlus gps;
    volatile bool transmittedFlag;
    volatile bool receivedFlag;

    void sendReceptionProto(const Reception &reception);
    void processGPSData(Reception &reception);
};