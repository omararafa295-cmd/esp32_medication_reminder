#pragma once

#include <Arduino.h>
#include "esp_sleep.h"
#include "AppConfig.h"

/**
 * ============================================================================
 * @class   PowerGuard
 * @brief   Manages ESP32 Power Domains, Deep Sleep entry, RTC wakeup sources,
 *          and hardware pin states during ultra-low-power sleep.
 * ============================================================================
 */
class PowerGuard {
public:
    /**
     * @brief Determines why the microcontroller woke up.
     * @return esp_sleep_wakeup_cause_t cause enum
     */
    static esp_sleep_wakeup_cause_t getWakeupCause();

    /**
     * @brief Converts the wakeup cause enum into a human-readable string.
     */
    static const char* getWakeupReasonString(esp_sleep_wakeup_cause_t cause);

    /**
     * @brief Waits until the stop button on GPIO 33 is released.
     * @param timeoutMs Maximum milliseconds to wait.
     * @return true if button was released; false if timed out.
     */
    static bool waitForButtonRelease(uint32_t timeoutMs);

    /**
     * @brief Enters ultra-low-power Deep Sleep mode.
     *        Arms the RTC timer for the designated duration in microseconds.
     *        This function never returns.
     * @param sleepMicroseconds Duration to remain asleep in microseconds.
     */
    static void enterDeepSleep(uint64_t sleepMicroseconds) __attribute__((noreturn));

    /**
     * @brief Prints power budget metrics and datasheet comparisons for project defense.
     */
    static void printPowerMetrics();
};
