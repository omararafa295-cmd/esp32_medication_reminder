#include "SoundAlerter.h"
#include <esp_arduino_version.h>

void SoundAlerter::begin() {
    // 1. Initialize Status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    setStatusLed(false);

    // 2. Initialize Hardware LEDC PWM on Buzzer Pin
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    // ESP32 Arduino Core 3.x+ API: channel is assigned automatically by pin
    if (!ledcAttach(PIN_PIEZO_BUZZER, TONE_PRIMARY_FREQ_HZ, PWM_LEDC_RESOLUTION_BITS)) {
        Serial.println("[LEDC] ERROR: ledcAttach failed on Buzzer Pin!");
    }
    ledcWriteTone(PIN_PIEZO_BUZZER, 0); // Start silenced
#else
    // Legacy ESP32 Arduino Core 2.x API
    ledcSetup(PWM_LEDC_CHANNEL, TONE_PRIMARY_FREQ_HZ, PWM_LEDC_RESOLUTION_BITS);
    ledcAttachPin(PIN_PIEZO_BUZZER, PWM_LEDC_CHANNEL);
    ledcWriteTone(PWM_LEDC_CHANNEL, 0);
#endif

    Serial.println("[HAL] Buzzer and Status LED initialized successfully.");
}

void SoundAlerter::writeHardwareTone(uint32_t frequencyHz) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWriteTone(PIN_PIEZO_BUZZER, frequencyHz);
#else
    ledcWriteTone(PWM_LEDC_CHANNEL, frequencyHz);
#endif
}

void SoundAlerter::setStatusLed(bool state) {
    digitalWrite(PIN_STATUS_LED, state ? HIGH : LOW);
}

void SoundAlerter::silenceAll() {
    writeHardwareTone(0);
    setStatusLed(false);
}

void SoundAlerter::playConfirmationChirp() {
    setStatusLed(true);
    writeHardwareTone(TONE_ACK_CHIRP_FREQ_HZ);
    vTaskDelay(pdMS_TO_TICKS(120));
    silenceAll();
}

void SoundAlerter::playConfigModeChirp() {
    for (int i = 0; i < 3; i++) {
        setStatusLed(true);
        writeHardwareTone(2200);
        delay(70);
        silenceAll();
        delay(70);
    }
}

void SoundAlerter::playTestBuzzerChirp() {
    setStatusLed(true);
    writeHardwareTone(TONE_PRIMARY_FREQ_HZ);
    delay(200);
    writeHardwareTone(TONE_SECONDARY_FREQ_HZ);
    delay(200);
    silenceAll();
}

/**
 * Helper to perform an interruptible delay.
 * Checks cancelFlag every 10ms to respond instantly to button clicks.
 */
static void interruptibleDelay(uint32_t totalMs, const volatile bool& activeFlag) {
    uint32_t elapsed = 0;
    while (activeFlag && elapsed < totalMs) {
        uint32_t slice = (totalMs - elapsed > 10) ? 10 : (totalMs - elapsed);
        vTaskDelay(pdMS_TO_TICKS(slice));
        elapsed += slice;
    }
}

void SoundAlerter::playAlternatingAlarmPattern(const volatile bool& cancelFlag) {
    if (!cancelFlag) {
        silenceAll();
        return;
    }

    // -------------------------------------------------------------
    // Phase 1: High Tone (2000 Hz) + Status LED ON (150 ms)
    // -------------------------------------------------------------
    setStatusLed(true);
    writeHardwareTone(TONE_PRIMARY_FREQ_HZ);
    interruptibleDelay(150, cancelFlag);

    if (!cancelFlag) { silenceAll(); return; }

    // -------------------------------------------------------------
    // Phase 2: Inter-tone Gap (Silence) + Status LED OFF (80 ms)
    // -------------------------------------------------------------
    silenceAll();
    interruptibleDelay(80, cancelFlag);

    if (!cancelFlag) { silenceAll(); return; }

    // -------------------------------------------------------------
    // Phase 3: Low Tone (1200 Hz) + Status LED ON (150 ms)
    // -------------------------------------------------------------
    setStatusLed(true);
    writeHardwareTone(TONE_SECONDARY_FREQ_HZ);
    interruptibleDelay(150, cancelFlag);

    if (!cancelFlag) { silenceAll(); return; }

    // -------------------------------------------------------------
    // Phase 4: Long Silence + Status LED OFF (600 ms)
    // -------------------------------------------------------------
    silenceAll();
    interruptibleDelay(600, cancelFlag);
}
