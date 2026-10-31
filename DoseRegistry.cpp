#include "DoseRegistry.h"
#include "AppConfig.h"

// ============================================================================
// RTC-RETAINED MEMORY REGION (Survives ESP32 Deep Sleep Cycles)
// ============================================================================
// Variables marked with RTC_DATA_ATTR reside in the 8 KB RTC Slow SRAM.
// While main system RAM (520 KB) is completely unpowered during Deep Sleep,
// RTC SRAM remains energized by the RTC Power Management Unit (PMU).
RTC_DATA_ATTR static uint32_t s_doseSequence      = 0;
RTC_DATA_ATTR static uint32_t s_dosesTaken         = 0;
RTC_DATA_ATTR static uint32_t s_dosesMissed        = 0;
RTC_DATA_ATTR static uint32_t s_bootCycles         = 0;
RTC_DATA_ATTR static uint32_t s_storageSignature   = 0;
RTC_DATA_ATTR static uint32_t s_sleepIntervalSec   = 0;

void DoseRegistry::initialize() {
    s_bootCycles++;

    // Detect if this is the very first power-on boot (cold boot)
    if (s_storageSignature != RTC_SIGNATURE_MAGIC) {
        s_storageSignature = RTC_SIGNATURE_MAGIC;
        s_doseSequence     = 0;
        s_dosesTaken        = 0;
        s_dosesMissed       = 0;
        s_sleepIntervalSec = (uint32_t)ACTIVE_SLEEP_DURATION_SEC;
        Serial.println("[RTC] Cold boot detected: Dose registry initialized.");
    } else {
        Serial.println("[RTC] Woke up from Deep Sleep: Retained data restored successfully.");
    }

    if (s_sleepIntervalSec == 0) {
        s_sleepIntervalSec = (uint32_t)ACTIVE_SLEEP_DURATION_SEC;
    }
}

void DoseRegistry::logDoseTriggered() {
    s_doseSequence++;
}

void DoseRegistry::logDoseTaken() {
    s_dosesTaken++;
}

void DoseRegistry::logDoseMissed() {
    s_dosesMissed++;
}

void DoseRegistry::resetStatistics() {
    s_doseSequence = 0;
    s_dosesTaken   = 0;
    s_dosesMissed  = 0;
}

uint32_t DoseRegistry::getDoseSequence() {
    return s_doseSequence;
}

uint32_t DoseRegistry::getTakenCount() {
    return s_dosesTaken;
}

uint32_t DoseRegistry::getMissedCount() {
    return s_dosesMissed;
}

uint32_t DoseRegistry::getBootCycles() {
    return s_bootCycles;
}

float DoseRegistry::getAdherenceRate() {
    if (s_doseSequence == 0) {
        return 100.0f;
    }
    return ((float)s_dosesTaken / (float)s_doseSequence) * 100.0f;
}

uint32_t DoseRegistry::getSleepIntervalSec() {
    if (s_sleepIntervalSec == 0) {
        s_sleepIntervalSec = (uint32_t)ACTIVE_SLEEP_DURATION_SEC;
    }
    return s_sleepIntervalSec;
}

void DoseRegistry::setSleepIntervalSec(uint32_t intervalSec) {
    if (intervalSec < 5) {
        intervalSec = 5; // Minimum 5 seconds
    }
    s_sleepIntervalSec = intervalSec;
    Serial.printf("[RTC] Sleep interval updated to %u seconds.\n", intervalSec);
}

void DoseRegistry::printStatusReport() {
    Serial.println("==================================================");
    Serial.println("       MEDICATION DOSE TRACKER (RTC REPORT)       ");
    Serial.println("==================================================");
    Serial.printf(" Total Wake Cycles : %u\n", s_bootCycles);
    Serial.printf(" Current Dose #    : %u\n", s_doseSequence);
    Serial.printf(" Confirmed (Taken) : %u\n", s_dosesTaken);
    Serial.printf(" Ignored (Missed)  : %u\n", s_dosesMissed);
    if (s_doseSequence > 0) {
        float adherence = ((float)s_dosesTaken / (float)s_doseSequence) * 100.0f;
        Serial.printf(" Adherence Rate    : %.1f %%\n", adherence);
    }
    Serial.println("==================================================");
}
