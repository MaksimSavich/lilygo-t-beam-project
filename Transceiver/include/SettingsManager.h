#pragma once
#include <LittleFS.h>
#include <RadioLib.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "packet.pb.h"

class SettingsManager
{
public:
    Settings config;

    explicit SettingsManager(SX1262 &radio);

    // Core functionality
    bool initialize();
    void load();
    void save();
    void print() const;
    void sendProto();

private:
    static constexpr const char *START_DELIMITER = "<START>";
    static constexpr const char *END_DELIMITER = "<END>";
    static constexpr size_t START_LEN = 7; // strlen("<START>")
    static constexpr size_t END_LEN = 5;   // strlen("<END>")

    SX1262 &radio;
    const char *filename = "/settings.bin";

    void initFilesystem();
    void createDefaults();
    bool validate() const;
};