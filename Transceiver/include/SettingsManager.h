/**
 * @file SettingsManager.h
 * @brief Header file for managing the settings of the SX1262 radio module.
 */

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
    Settings mConfig;

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
    static constexpr size_t START_LEN = 7; ///< Length of the start delimiter
    static constexpr size_t END_LEN = 5;   ///< Length of the end delimiter

    SX1262 &mRadio;                          ///< Reference to the SX1262 radio module
    const char *mFilename = "/settings.bin"; ///< Filename for storing settings

    void initFilesystem();
    void createDefaults();
    bool validate() const;
};