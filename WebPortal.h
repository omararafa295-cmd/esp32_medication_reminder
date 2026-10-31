#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * @class   WebPortal
 * @brief   Provides a local Wi-Fi Access Point and an embedded interactive
 *          Medical Dashboard for real-time telemetry, schedule configuration,
 *          and system testing directly from a mobile phone or laptop browser.
 * ============================================================================
 */
class WebPortal {
public:
    /**
     * @brief Launches the Wi-Fi Access Point, starts the HTTP Web Server,
     *        and serves the dashboard until the user saves or times out.
     */
    static void startPortal();

    /**
     * @brief Checks if Web Portal mode is currently active.
     */
    static bool isActive();

    /**
     * @brief Shuts down the AP and returns to Deep Sleep.
     */
    static void stopPortalAndSleep();
};
