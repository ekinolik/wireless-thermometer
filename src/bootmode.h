#pragma once
#include <Arduino.h>

class BootMode {
public:
    struct Config {
        uint32_t stableAfterMs = 10000;

        uint8_t hardBootsToForceFactoryReset = 3;

        const char* nvsNamespace = "boot";
        const char* keyHardCount = "hc";
        const char* keyStable = "stable";
    };

    BootMode();
    explicit BootMode(const Config& cfg);

    enum class Action { None, FactoryReset };

    Action begin();

    void loop();

    void resetWindow();

private:
    Config cfg_;
    bool clearedAfterStable_ = false;
    uint32_t bootMs_ = 0;

    void clearIfStable_();

    uint8_t readHardCount_();
    bool readStable_();
    void writeHardCount_(uint8_t v);
    void writeStable_(bool v);
};