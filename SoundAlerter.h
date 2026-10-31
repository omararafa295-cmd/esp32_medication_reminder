#pragma once

#include <Arduino.h>
#include "AppConfig.h"

/**
 * ============================================================================
 * @class   SoundAlerter
 * @brief   Drives the Piezo Buzzer via Hardware LEDC PWM and synchronizes the
 *          Status LED to generate a standard alternating medical alarm tone.
 * ============================================================================
 */
class SoundAlerter {
public:
    /**
     * @brief Initializes GPIO pins and Hardware LEDC PWM timer.
     */
    static void begin();

    /**
     * @brief Emits a single complete medical alert cycle (2000Hz -> pause -> 1200Hz -> pause)
     *        with synchronous Status LED illumination.
     * @param cancelFlag Reference to an atomic boolean flag. If becomes false, stops early.
     */
    static void playAlternatingAlarmPattern(const volatile bool& cancelFlag);

    /**
     * @brief Plays a brief high-pitched chirp confirming that dose was acknowledged.
     */
    static void playConfirmationChirp();

    /**
     * @brief Plays sensory feedback (3 rapid blinks and tone) entering Config Portal.
     */
    static void playConfigModeChirp();

    /**
     * @brief Plays a short test alert triggered from the web dashboard.
     */
    static void playTestBuzzerChirp();

    /**
     * @brief Immediately silences the buzzer and turns off the status LED.
     */
    static void silenceAll();

private:
    static void writeHardwareTone(uint32_t frequencyHz);
    static void setStatusLed(bool state);
};
