#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * @file    AppConfig.h
 * @brief   System configuration, pin mapping, timing and acoustic parameters
 * @project Timer-Based Medication Reminder for ESP32
 * ============================================================================
 */

// ============================================================================
// 1. HARDWARE PIN DEFINITIONS (Pin Mapping)
// ============================================================================

/**
 * @brief Push Button (Stop / Acknowledge Alarm)
 * Connected to GPIO 33. This pin belongs to the RTC GPIO domain (RTC_GPIO8),
 * allowing it to configure internal pullup resistors and edge-triggered interrupts.
 */
#define PIN_STOP_BUTTON             33

/**
 * @brief Piezo Buzzer (Passive)
 * Connected to GPIO 25. Driven by ESP32 Hardware LEDC PWM timer to produce
 * pure acoustic frequencies without blocking the CPU cores.
 */
#define PIN_PIEZO_BUZZER            25

/**
 * @brief Status Indicator LED
 * Connected to GPIO 2 (also corresponds to the onboard LED on most ESP32 DevKits).
 * Flashes in strict synchrony with the medical alternating tone.
 */
#define PIN_STATUS_LED              2

/**
 * @brief LED Logic Level Polarity
 * Set to true for Active-HIGH (most external LEDs & standard ESP32 boards).
 * Set to false if using an Active-LOW onboard LED.
 */
#define LED_ACTIVE_HIGH             true

// ============================================================================
// 2. HARDWARE LEDC PWM & ACOUSTIC TONE SETTINGS
// ============================================================================

#define TONE_PRIMARY_FREQ_HZ        2000    ///< First medical tone frequency (2.0 kHz)
#define TONE_SECONDARY_FREQ_HZ      1200    ///< Second medical tone frequency (1.2 kHz)
#define TONE_ACK_CHIRP_FREQ_HZ      2500    ///< Brief positive acknowledgment chirp (2.5 kHz)

#define PWM_LEDC_RESOLUTION_BITS    10      ///< 10-bit PWM resolution (0 - 1023 duty cycle)
#define PWM_LEDC_CHANNEL            0       ///< LEDC channel (used in ESP32 Core 2.x fallback)

// ============================================================================
// 3. SLEEP DURATION & TIMING PROFILE (Deep Sleep Management)
// ============================================================================

/**
 * @brief Demonstration vs Clinical Mode Selection
 * Set to 'true' for lab testing and video presentation (15 seconds sleep).
 * Set to 'false' for actual medical usage (e.g. 8 hours between doses).
 */
#define DEMO_MODE_ACTIVE            true

/**
 * @brief Default Demonstration Interval (in Seconds)
 * Set to 15 seconds to allow fast testing, grading, and video recording.
 */
#define DEMO_SLEEP_INTERVAL_SEC     15ULL

/**
 * @brief Clinical Medication Schedule Interval (in Hours)
 * Typical prescription interval (e.g., every 8 hours = 3 times daily).
 */
#define CLINICAL_SCHEDULE_HOURS     8ULL

/**
 * @brief Computed Sleep Interval (in Microseconds for esp_sleep_enable_timer_wakeup)
 */
#define ACTIVE_SLEEP_DURATION_SEC   (DEMO_MODE_ACTIVE ? DEMO_SLEEP_INTERVAL_SEC : (CLINICAL_SCHEDULE_HOURS * 3600ULL))
#define ACTIVE_SLEEP_DURATION_US    (ACTIVE_SLEEP_DURATION_SEC * 1000000ULL)
#define CURRENT_SLEEP_SEC           ACTIVE_SLEEP_DURATION_SEC
#define CURRENT_SLEEP_US            ACTIVE_SLEEP_DURATION_US

/**
 * @brief Alarm Timeout Configuration (Adaptive Battery Guard)
 * - In Demo / Fast Lab Test mode (interval <= 60s): 10 seconds is optimal for testing
 *   and recording without prolonged noisy beeping on the desk.
 * - In Clinical mode (interval in hours): 60 seconds maximum timeout.
 */
#define DEMO_ALARM_TIMEOUT_SEC      10      ///< 10 seconds timeout for fast video demo
#define CLINICAL_ALARM_TIMEOUT_SEC  60      ///< 60 seconds battery guard in real use
#define MAX_ALARM_WINDOW_SEC        CLINICAL_ALARM_TIMEOUT_SEC

/**
 * @brief Hardware Button Software Debounce (in Milliseconds)
 * Rejects mechanical contact bouncing noise via tick difference filtering.
 */
#define BUTTON_DEBOUNCE_MS          200

/**
 * @brief Maximum Wait for Button Release (in Milliseconds)
 * Prevents the microcontroller from entering deep sleep while button is still pressed.
 */
#define BUTTON_RELEASE_TIMEOUT_MS   5000

// ============================================================================
// 4. WEB CONFIGURATION PORTAL (Access Point & Local Dashboard)
// ============================================================================
/**
 * @brief Button Long-Press Duration to Trigger Web Portal (3000 ms = 3 seconds)
 */
#define BUTTON_LONG_PRESS_MS        3000

/**
 * @brief Local Wi-Fi Access Point Credentials
 */
#define AP_WIFI_SSID                "MedReminder-Config"
#define AP_WIFI_PASSWORD            ""             // Open network for effortless mobile connection
#define AP_WIFI_CHANNEL             1
#define AP_MAX_CLIENTS              4

/**
 * @brief Web Portal Inactivity Timeout (3 minutes = 180,000 ms)
 * Automatically returns the ESP32 to Deep Sleep if user leaves without saving.
 */
#define AP_PORTAL_TIMEOUT_MS        180000

/**
 * @brief Serial Baud Rate for Debug Diagnostics
 */
#define DEBUG_SERIAL_BAUD           115200
