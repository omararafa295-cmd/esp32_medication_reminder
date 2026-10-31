#include "PowerGuard.h"
#include "SoundAlerter.h"
#include "driver/rtc_io.h"

esp_sleep_wakeup_cause_t PowerGuard::getWakeupCause() {
    return esp_sleep_get_wakeup_cause();
}

const char* PowerGuard::getWakeupReasonString(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:    return "RTC Timer Expired (Medication Due)";
        case ESP_SLEEP_WAKEUP_EXT0:     return "External RTC Pin (GPIO 33 Pressed)";
        case ESP_SLEEP_WAKEUP_EXT1:     return "External Pin Group (EXT1)";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Touchpad Sensor";
        case ESP_SLEEP_WAKEUP_ULP:      return "ULP Co-processor";
        default:                        return "Cold Boot / Hard Reset / Power-On";
    }
}

bool PowerGuard::waitForButtonRelease(uint32_t timeoutMs) {
    if (digitalRead(PIN_STOP_BUTTON) != LOW) {
        return true;
    }

    uint32_t startTime = millis();
    while (digitalRead(PIN_STOP_BUTTON) == LOW) {
        if ((millis() - startTime) >= timeoutMs) {
            Serial.println("[POWER] Warning: Button was still depressed after timeout.");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Contact bounce stabilization
    return (digitalRead(PIN_STOP_BUTTON) != LOW);
}

void PowerGuard::printPowerMetrics() {
    // 3.3V operating voltage
    const float V_SUPPLY = 3.3f;
    
    // Datasheet current values (ESP32 Series Datasheet & Technical Reference)
    const float I_ACTIVE_MA     = 240.0f; // Dual-core CPU running @ 240 MHz with peripherals
    const float I_MODEM_OFF_MA  = 30.0f;  // CPU active, Wi-Fi/BT radios powered down
    const float I_DEEP_SLEEP_UA = 10.0f;  // RTC Controller + 8KB RTC SRAM active (0.010 mA)

    Serial.println("\n--------------------------------------------------");
    Serial.println("         ESP32 POWER CONSUMPTION ANALYSIS         ");
    Serial.println("--------------------------------------------------");
    Serial.printf(" 1. Full Active Mode (Radios ON) : ~%.1f mA | ~%.1f mW\n",
                  I_ACTIVE_MA, (V_SUPPLY * I_ACTIVE_MA));
    Serial.printf(" 2. Alarm Active (Modem OFF)     : ~%.1f mA | ~%.1f mW\n",
                  I_MODEM_OFF_MA, (V_SUPPLY * I_MODEM_OFF_MA));
    Serial.printf(" 3. Deep Sleep (Timer + RTC SRAM): ~%.1f uA | ~%.3f mW\n",
                  I_DEEP_SLEEP_UA, (V_SUPPLY * (I_DEEP_SLEEP_UA / 1000.0f)));
    Serial.println(" Deep Sleep cuts energy consumption by > 99.9%!");
    Serial.println("--------------------------------------------------\n");
}

void PowerGuard::enterDeepSleep(uint64_t sleepMicroseconds) {
    Serial.println("\n[POWER] Preparing system for ultra-low-power Deep Sleep...");

    // 1. Ensure outputs are muted and LEDs are dark
    SoundAlerter::silenceAll();

    // 2. Detach digital edge interrupts
    detachInterrupt(digitalPinToInterrupt(PIN_STOP_BUTTON));

    // 3. Wait for patient to physically release the button
    waitForButtonRelease(BUTTON_RELEASE_TIMEOUT_MS);

    // 4. Configure RTC Domain GPIO internal pullup
    // When main digital domain is powered down, the RTC domain maintains the pullup
    rtc_gpio_init((gpio_num_t)PIN_STOP_BUTTON);
    rtc_gpio_set_direction((gpio_num_t)PIN_STOP_BUTTON, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)PIN_STOP_BUTTON);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_STOP_BUTTON);

    // 5. Arm internal RTC Timer wake-up & Button Wake-up (EXT0)
    esp_sleep_enable_timer_wakeup(sleepMicroseconds);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_STOP_BUTTON, 0); // Active LOW wakeup

    uint32_t sleepSeconds = (uint32_t)(sleepMicroseconds / 1000000ULL);
    Serial.printf("[POWER] Deep Sleep timer armed for: %u seconds (%llu us).\n",
                  sleepSeconds, (unsigned long long)sleepMicroseconds);

    printPowerMetrics();

    Serial.println("[POWER] Microcontroller is entering Deep Sleep now. Zzz...\n");
    Serial.flush(); // Flush TX buffer to ensure all debug bytes are transmitted

    // 6. Enter Deep Sleep (Main CPU powers down, execution terminates)
    esp_deep_sleep_start();

    // Code will never execute beyond this point
    while (true) {
        // Halt
    }
}
