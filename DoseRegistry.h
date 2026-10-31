#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * @class   DoseRegistry
 * @brief   Encapsulates medication tracking statistics retained across Deep Sleep
 *          in the ESP32's 8 KB RTC Slow SRAM (RTC_DATA_ATTR domain).
 * ============================================================================
 */
class DoseRegistry {
public:
    /**
     * @brief Initializes the dose registry on cold boot and maintains data across sleep.
     */
    static void initialize();

    /**
     * @brief Increments the current scheduled dose sequence count.
     */
    static void logDoseTriggered();

    /**
     * @brief Marks current dose as successfully taken/acknowledged by user.
     */
    static void logDoseTaken();

    /**
     * @brief Marks current dose as missed (timed out after 60 seconds).
     */
    static void logDoseMissed();

    /**
     * @brief Resets all statistics back to zero.
     */
    static void resetStatistics();

    // Getter methods
    static uint32_t getDoseSequence();
    static uint32_t getTakenCount();
    static uint32_t getMissedCount();
    static uint32_t getBootCycles();
    static float    getAdherenceRate();

    // Sleep interval configuration
    static uint32_t getSleepIntervalSec();
    static void     setSleepIntervalSec(uint32_t intervalSec);

    /**
     * @brief Prints a formatted diagnostic summary to the Serial interface.
     */
    static void printStatusReport();

private:
    static const uint32_t RTC_SIGNATURE_MAGIC = 0x5254434D; // "RTCM"
};
