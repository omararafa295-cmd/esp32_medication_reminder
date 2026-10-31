/**
 * ============================================================================
 * @file    esp32_medication_reminder.ino
 * @brief   Main Application Entry Point & FreeRTOS Architecture
 * @project Timer-Based Medication Reminder for ESP32
 * ============================================================================
 * Architecture Overview:
 * 1. AppConfig.h     : Hardware pin maps, acoustic parameters, and sleep intervals.
 * 2. DoseRegistry    : RTC-retained memory attributes for doses and adherence.
 * 3. SoundAlerter    : Hardware LEDC PWM buzzer driver and synchronized status LED.
 * 4. PowerGuard      : Deep Sleep transitions, RTC GPIO pullups, and power budgets.
 * 5. FreeRTOS Tasks  : Non-blocking alarm generator and supervisor state machine.
 * ============================================================================
 */

#include <Arduino.h>
#include "AppConfig.h"
#include "DoseRegistry.h"
#include "SoundAlerter.h"
#include "PowerGuard.h"
#include "WebPortal.h"
#include "driver/rtc_io.h"

// ============================================================================
// FREERTOS OBJECTS & GLOBAL SYNCHRONIZATION FLAGS
// ============================================================================
// Binary semaphore signaled by the button hardware ISR
static SemaphoreHandle_t g_silenceTriggerSemaphore = NULL;

// Atomic flag controlling the alarm audio/visual task loop
static volatile bool g_isAlarmActive = false;

// Task handles
static TaskHandle_t g_handleAlarmTask      = NULL;
static TaskHandle_t g_handleSupervisorTask = NULL;

// ============================================================================
// HARDWARE EXTERNAL INTERRUPT SERVICE ROUTINE (IRAM_ATTR)
// ============================================================================
/**
 * @brief Hardware ISR attached to GPIO 33 (FALLING edge).
 *        Decorated with IRAM_ATTR to ensure execution from internal fast SRAM
 *        without flash cache cache-miss latency.
 *        Implements tick-based software debounce (200 ms).
 */
static void IRAM_ATTR isrStopButtonPressed() {
    if (g_silenceTriggerSemaphore == NULL) {
        return;
    }

    uint32_t currentTick = xTaskGetTickCountFromISR();
    static uint32_t lastInterruptTick = 0;

    // Check if 200 ms debounce window has elapsed
    if ((currentTick - lastInterruptTick) > pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        lastInterruptTick = currentTick;

        BaseType_t higherPriorityTaskWoken = pdFALSE;
        // Release binary semaphore to unblock the supervisor task
        xSemaphoreGiveFromISR(g_silenceTriggerSemaphore, &higherPriorityTaskWoken);

        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// ============================================================================
// FREERTOS TASK 1: ACOUSTIC & VISUAL ALARM GENERATOR
// ============================================================================
/**
 * @brief Independent FreeRTOS task responsible for generating the alternating
 *        medical sound pattern (2000 Hz <-> 1200 Hz) and flashing the LED.
 *        Terminates the sound pattern cleanly as soon as g_isAlarmActive becomes false.
 */
static void TaskAlarmPatternGenerator(void* pvParameters) {
    (void)pvParameters;

    while (g_isAlarmActive) {
        SoundAlerter::playAlternatingAlarmPattern(g_isAlarmActive);
    }

    SoundAlerter::silenceAll();
    vTaskDelete(NULL); // Cleanly self-delete task once alarm is deactivated
}

// ============================================================================
// FREERTOS TASK 2: SYSTEM SUPERVISOR & STATE MACHINE
// ============================================================================
/**
 * @brief Supervises the active alarm session.
 *        Listens for:
 *        1) Button press event via binary semaphore (Immediate Silence / TAKEN).
 *        2) Maximum 60-second timeout guard (Battery Protection / MISSED).
 *        Once resolved, triggers the transition back to ultra-low-power Deep Sleep.
 */
static void TaskRemindSupervisor(void* pvParameters) {
    (void)pvParameters;

    const uint32_t currentInterval = DoseRegistry::getSleepIntervalSec();
    // Adaptive Timeout: 10s for fast testing (<= 60s), 60s for clinical schedules (> 60s)
    const uint32_t timeoutSec      = (currentInterval <= 60) ? DEMO_ALARM_TIMEOUT_SEC : CLINICAL_ALARM_TIMEOUT_SEC;
    const uint32_t timeoutMs       = timeoutSec * 1000UL;
    const uint32_t startTimeMs     = millis();
    bool doseAcknowledgedByUser     = false;

    Serial.printf("[SUPERVISOR] Alarm ringing (Timeout set to %u s). Monitoring button & timeout...\n", timeoutSec);

    while (g_isAlarmActive) {
        // Wait on the binary semaphore with a short 50 ms polling timeout
        if (xSemaphoreTake(g_silenceTriggerSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Check if user is performing a Long Press (> 3 seconds) to enter Web Portal
            uint32_t pressStart = millis();
            while (digitalRead(PIN_STOP_BUTTON) == LOW) {
                if ((millis() - pressStart) >= BUTTON_LONG_PRESS_MS) {
                    Serial.println("\n>>> [USER ACTION] Long Press (3s) -> Launching Web Config Portal!");
                    g_isAlarmActive = false;
                    SoundAlerter::silenceAll();
                    DoseRegistry::logDoseTaken();
                    SoundAlerter::playConfigModeChirp();
                    WebPortal::startPortal();
                    vTaskDelete(NULL);
                    return;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            // Normal short tap: confirm dose
            doseAcknowledgedByUser = true;
            g_isAlarmActive = false; // Stop alarm pattern
            Serial.println("\n>>> [USER ACTION] Stop Button Pressed (GPIO 33)!");
            break;
        }

        // Event 2: Check if auto-timeout expired
        if ((millis() - startTimeMs) >= timeoutMs) {
            // Timed out: user was not present
            doseAcknowledgedByUser = false;
            g_isAlarmActive = false; // Stop alarm pattern
            Serial.printf("\n>>> [TIMEOUT] %u-Second Window Expired without confirmation.\n", timeoutSec);
            break;
        }
    }

    // Ensure audio & visual actuators are completely silenced
    SoundAlerter::silenceAll();

    // Small delay to allow TaskAlarmPatternGenerator to exit cleanly
    vTaskDelay(pdMS_TO_TICKS(100));

    // Update RTC-retained dose statistics
    if (doseAcknowledgedByUser) {
        DoseRegistry::logDoseTaken();
        Serial.println("[STATUS] Result: DOSE CONFIRMED (TAKEN).");
        // Play brief positive confirmation chirp
        SoundAlerter::playConfirmationChirp();
    } else {
        DoseRegistry::logDoseMissed();
        Serial.println("[STATUS] Result: DOSE UNANSWERED (MISSED).");
    }

    // Display comprehensive report to Serial Monitor
    DoseRegistry::printStatusReport();

    // Return to Deep Sleep using the configured schedule interval
    uint32_t sleepInterval = DoseRegistry::getSleepIntervalSec();
    PowerGuard::enterDeepSleep((uint64_t)sleepInterval * 1000000ULL);

    vTaskDelete(NULL);
}

// ============================================================================
// ARDUINO SETUP: HARDWARE BOOTSTRAP & STATE DISPATCHER
// ============================================================================
void setup() {
    // 1. Initialize Serial Monitor for system diagnostics
    Serial.begin(DEBUG_SERIAL_BAUD);
    delay(200); // Allow UART hardware buffer to settle

    Serial.println("\n==================================================");
    Serial.println("  ESP32 TIMER-BASED MEDICATION REMINDER SYSTEM   ");
    Serial.println("  Clean OOP Architecture + FreeRTOS + Deep Sleep ");
    Serial.println("==================================================");

    // 2. Release GPIO 33 from RTC power domain back to Digital GPIO matrix
    rtc_gpio_deinit((gpio_num_t)PIN_STOP_BUTTON);

    // 3. Configure digital GPIO pins
    pinMode(PIN_STOP_BUTTON, INPUT_PULLUP);

    // 4. Initialize Hardware Peripherals (LEDC PWM & Status LED)
    SoundAlerter::begin();

    // 5. Initialize or restore RTC Retained Data
    DoseRegistry::initialize();

    // 6. Inspect Deep Sleep Wakeup Cause
    esp_sleep_wakeup_cause_t wakeReason = PowerGuard::getWakeupCause();
    Serial.printf("[BOOT] Wakeup Reason : %s (Code %d)\n",
                  PowerGuard::getWakeupReasonString(wakeReason), (int)wakeReason);

    // 7. Determine Application Flow based on Wakeup Source
    if (wakeReason == ESP_SLEEP_WAKEUP_TIMER) {
        // --------------------------------------------------------------------
        // SCENARIO A: Woken by RTC Timer -> It is time for medication!
        // --------------------------------------------------------------------
        Serial.println("[ALARM] Scheduled medication interval elapsed. Triggering alert!");

        // Increment scheduled dose count in RTC memory
        DoseRegistry::logDoseTriggered();

        // Create FreeRTOS Binary Semaphore for the button ISR
        g_silenceTriggerSemaphore = xSemaphoreCreateBinary();

        // Attach external FALLING interrupt to GPIO 33
        attachInterrupt(digitalPinToInterrupt(PIN_STOP_BUTTON), isrStopButtonPressed, FALLING);

        // Arm the alarm active flag
        g_isAlarmActive = true;

        // Launch FreeRTOS Tasks pinned to Core 1
        xTaskCreatePinnedToCore(
            TaskAlarmPatternGenerator,
            "AlarmToneTask",
            2048,
            NULL,
            2, // Priority 2
            &g_handleAlarmTask,
            1  // Run on Core 1
        );

        xTaskCreatePinnedToCore(
            TaskRemindSupervisor,
            "SupervisorTask",
            3072,
            NULL,
            3, // Priority 3 (higher priority to respond quickly to button)
            &g_handleSupervisorTask,
            1  // Run on Core 1
        );

    } else if (wakeReason == ESP_SLEEP_WAKEUP_EXT0) {
        // --------------------------------------------------------------------
        // SCENARIO B: Woken by Button (GPIO 33) while asleep in Deep Sleep
        // --------------------------------------------------------------------
        Serial.println("[WAKE] Button wake-up from Deep Sleep. Checking press duration...");

        uint32_t holdStart = millis();
        while (digitalRead(PIN_STOP_BUTTON) == LOW) {
            if ((millis() - holdStart) >= BUTTON_LONG_PRESS_MS) {
                // User held button for > 3 seconds -> Enter Local Web Config Portal!
                Serial.println("[WAKE] Long Press (3s) detected -> Launching Web Config Portal!");
                SoundAlerter::playConfigModeChirp();
                WebPortal::startPortal();
                return; // startPortal never returns; enters deep sleep upon exit
            }
            delay(20);
        }

        // Button released before 3 seconds (quick tap while asleep)
        Serial.println("[WAKE] Short tap while asleep -> Showing status and returning to sleep.");
        SoundAlerter::playConfirmationChirp();
        DoseRegistry::printStatusReport();

        uint32_t sleepInterval = DoseRegistry::getSleepIntervalSec();
        PowerGuard::enterDeepSleep((uint64_t)sleepInterval * 1000000ULL);

    } else {
        // --------------------------------------------------------------------
        // SCENARIO C: Cold Boot / Hard Reset / First Power-On
        // --------------------------------------------------------------------
        Serial.println("[SYSTEM] Cold boot detected. Initializing first medication cycle...");

        // Check if user is holding the button on initial power-up
        if (digitalRead(PIN_STOP_BUTTON) == LOW) {
            uint32_t holdStart = millis();
            while (digitalRead(PIN_STOP_BUTTON) == LOW) {
                if ((millis() - holdStart) >= BUTTON_LONG_PRESS_MS) {
                    Serial.println("[BOOT] Button held on boot -> Launching Web Config Portal!");
                    SoundAlerter::playConfigModeChirp();
                    WebPortal::startPortal();
                    return;
                }
                delay(20);
            }
        }

        // Play a quick self-test chirp to verify buzzer & LED hardware
        SoundAlerter::playConfirmationChirp();

        DoseRegistry::printStatusReport();

        uint32_t initialInterval = DoseRegistry::getSleepIntervalSec();
        Serial.printf("[SYSTEM] Entering initial Deep Sleep for %u seconds...\n", initialInterval);

        // Enter Deep Sleep using the stored/default schedule interval
        PowerGuard::enterDeepSleep((uint64_t)initialInterval * 1000000ULL);
    }
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================
void loop() {
    // Execution is handled completely by FreeRTOS tasks and hardware interrupts.
    // Sleep this low-priority idle task to conserve CPU time.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
