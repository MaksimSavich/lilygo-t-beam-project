#pragma once
#include <RadioLib.h>
#include <TinyGPS++.h>
#include "SettingsManager.h"
#include "packet.pb.h"
#include "LoraBoards.h"

class RadioManager
{
public:
    RadioManager(SX1262 &radio, HardwareSerial &gpsSerial);
    bool initialize(SettingsManager &settings);
    bool configure(const SettingsManager &settings);
    void transmit(const uint8_t *data, size_t length);
    void TxSerialGPSPacket();
    void startReceive();
    void processReceptionLog();
    void processTransmitLog(int);
    void handleTransmitted() { transmittedFlag = true; }
    void handleReceived() { receivedFlag = true; }
    bool isTransmitted() const { return transmittedFlag; }
    bool isReceived() const { return receivedFlag; }
    State getState() { return state; }
    void setState(State newState);
    void standby() { radio.standby(); };

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
    State state = State_STANDBY;
    volatile bool transmittedFlag;
    volatile bool receivedFlag;

    void TxSerialLogPacket(const Log &log);
    void ProcessGPSData(Gps &gps);
};