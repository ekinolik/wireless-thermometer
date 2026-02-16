#include "bootmode.h"
#include <Preferences.h>

RTC_DATA_ATTR uint32_t rtc_magic = 0;
RTC_DATA_ATTR uint8_t rtc_softBootCount = 0;

static constexpr uint32_t RTC_MAGIC_VALUE = 0xB007B007;

static Preferences prefs;

BootMode::BootMode() : cfg_(Config{}) {}
BootMode::BootMode(const Config& cfg) : cfg_(cfg) {}

uint8_t BootMode::readHardCount_() {
    return prefs.getUChar(cfg_.keyHardCount, 0);
}

bool BootMode::readStable_() {
    return prefs.getBool(cfg_.keyStable, true);
}

void BootMode::writeHardCount_(uint8_t v) {
    prefs.putUChar(cfg_.keyHardCount, v);
}

void BootMode::writeStable_(bool v) {
    prefs.putBool(cfg_.keyStable, v);
}

BootMode::Action BootMode::begin() {
    bootMs_ = millis();
    clearedAfterStable_ = false;

    prefs.begin(cfg_.nvsNamespace, false);

    const bool rtcValid = (rtc_magic == RTC_MAGIC_VALUE);

    if (rtcValid) {
        // Soft reset path, avoids NVS writes.
        rtc_softBootCount++;
        Serial.printf("Boot mode: soft reset detected (RTC). softBootCount=%u\n", rtc_softBootCount);
    } else {
        // Hard boot path
        rtc_magic = RTC_MAGIC_VALUE;
        rtc_softBootCount = 0;

        bool wasStable = readStable_();
        uint8_t hardCount = readHardCount_();

        if (wasStable) {
            hardCount = 1;
            writeStable_(false);
            writeHardCount_(hardCount);
        } else if ( hardCount < cfg_.hardBootsToForceFactoryReset ) {
            hardCount++;
            writeHardCount_(hardCount);
            //hardCount = (uint8_t)min<int>(hardCount + 1, 255);
        }

        Serial.printf("BootMode: hard boot detected. hardCount=%u (stable=%s)\n",
            hardCount, wasStable ? "true" : "false"
        );

        if (hardCount >= cfg_.hardBootsToForceFactoryReset) {
            Serial.println("BootMode: triggering FACTORY RESET due to rapid hard reboots.");
            return Action::FactoryReset;
        }
    }

    return Action::None;
}

void BootMode::clearIfStable_() {
    if (clearedAfterStable_) return;

    if (millis() - bootMs_ < cfg_.stableAfterMs) {
        return;
        // Up long enough
    }

    writeHardCount_(0);
    writeStable_(true);

    // Also reset RTC
    rtc_softBootCount = 0;

    clearedAfterStable_ = true;
    bool stableValue_ = readStable_();
    Serial.println("BootMode: stable reached; cleared rapid boot window.");
}

void BootMode::loop() {
    clearIfStable_();
}

void BootMode::resetWindow() {
    writeHardCount_(0);
    writeStable_(true);
    rtc_softBootCount = 0;
    clearedAfterStable_ = true;
    Serial.println("BootMode: window reset.");
}