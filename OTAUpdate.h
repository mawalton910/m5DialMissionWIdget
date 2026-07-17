// OTAUpdate.h - Remote GitHub OTA Update for M5 Dial
#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <M5Dial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h>
#include "secrets.h"

// ============================================
// OTA UPDATE CONFIGURATION
// ============================================

// Special RFID UIDs that trigger OTA update (add your OTA trigger tags here)
const String OTA_TRIGGER_UIDS[] = {
    "04B6C5D65F6180",      // OTA trigger tag
    // Add more OTA trigger tags as needed
};
const int OTA_TRIGGER_UIDS_COUNT = sizeof(OTA_TRIGGER_UIDS) / sizeof(OTA_TRIGGER_UIDS[0]);

// GitHub OTA settings
const char* OTA_WIFI_SSID = DEFAULT_WIFI_SSID;            // WiFi SSID for OTA updates
const char* OTA_WIFI_PASSWORD = DEFAULT_WIFI_PASSWORD;    // WiFi password for OTA updates
String targetBranch = "main";                     // GitHub branch to download from

// GitHub repository info
const char* GITHUB_USER = "mawalton910";
const char* GITHUB_REPO = "mission";
const char* FIRMWARE_FILENAME = "update.ino.bin";

// Build GitHub URL dynamically
String getOTAUrl() {
    return "https://raw.githubusercontent.com/" + 
           String(GITHUB_USER) + "/" + 
           String(GITHUB_REPO) + "/" + 
           targetBranch + "/" + 
           String(FIRMWARE_FILENAME);
}

// ============================================
// OTA PROGRESS DISPLAY
// ============================================

void displayOTAProgress(int progress, int total) {
    M5Dial.Display.fillScreen(BLACK);
    M5Dial.Display.setTextColor(CYAN, BLACK);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextDatum(middle_center);
    
    // Calculate percentage
    int percentage = (progress * 100) / total;
    
    // Display main message
    M5Dial.Display.drawString("DOWNLOADING", 120, 60);
    M5Dial.Display.drawString("FIRMWARE...", 120, 85);
    
    // Display percentage
    M5Dial.Display.setTextSize(3);
    M5Dial.Display.setTextColor(GREEN, BLACK);
    M5Dial.Display.drawString(String(percentage) + "%", 120, 130);
    
    // Draw progress bar
    int barWidth = 180;
    int barHeight = 20;
    int barX = (240 - barWidth) / 2;
    int barY = 170;
    
    // Draw border
    M5Dial.Display.drawRect(barX, barY, barWidth, barHeight, WHITE);
    
    // Fill progress
    int fillWidth = (barWidth - 4) * percentage / 100;
    M5Dial.Display.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, GREEN);
    
    // Display bytes
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(WHITE, BLACK);
    String byteInfo = String(progress / 1024) + "KB / " + String(total / 1024) + "KB";
    M5Dial.Display.drawString(byteInfo, 120, 200);
}

void displayOTAStatus(String message, uint16_t color) {
    M5Dial.Display.fillScreen(BLACK);
    M5Dial.Display.setTextColor(color, BLACK);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.drawString(message, 120, 120);
}

// ============================================
// OTA UPDATE CORE FUNCTION
// ============================================

void triggerGitHubUpdate() {
    Serial.println("=== OTA UPDATE TRIGGERED ===");
    
    // Disable watchdog timer during update
    esp_task_wdt_deinit();
    
    // Display connecting message
    displayOTAStatus("CONNECTING TO", CYAN);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("OTA WIFI...", 120, 150);
    
    // Disconnect from current WiFi
    WiFi.disconnect(true);
    delay(500);
    
    // Connect to OTA WiFi
    WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
    Serial.println("Connecting to OTA WiFi: " + String(OTA_WIFI_SSID));
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        attempts++;
        Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        displayOTAStatus("WIFI FAILED", RED);
        Serial.println("\nOTA WiFi connection failed!");
        delay(3000);
        ESP.restart();
        return;
    }
    
    Serial.println("\nOTA WiFi Connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    
    // Build firmware URL
    String firmwareUrl = getOTAUrl();
    Serial.println("Firmware URL: " + firmwareUrl);
    
    displayOTAStatus("PREPARING", YELLOW);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("UPDATE...", 120, 150);
    delay(1000);
    
    // Configure WiFiClientSecure
    WiFiClientSecure client;
    // SECURITY WARNING: TLS certificate validation is disabled. A network attacker could
    // serve malicious firmware. Enable certificate pinning or hash verification before
    // deploying to a shared or untrusted network (S2).
    client.setInsecure();  // Bypass certificate validation (GitHub SSL)
    
    // Set up progress callback
    httpUpdate.onProgress([](int current, int total) {
        displayOTAProgress(current, total);
        Serial.printf("Progress: %d%%\r", (current * 100) / total);
    });
    
    // Perform the update
    Serial.println("Starting HTTP Update...");
    displayOTAStatus("DOWNLOADING", GREEN);
    
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);
    
    // Handle update result
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("Update FAILED (%d): %s\n", 
                         httpUpdate.getLastError(), 
                         httpUpdate.getLastErrorString().c_str());
            displayOTAStatus("UPDATE FAILED", RED);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString(httpUpdate.getLastErrorString(), 120, 150);
            delay(5000);
            ESP.restart();
            break;
            
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No updates available");
            displayOTAStatus("NO UPDATE", YELLOW);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString("AVAILABLE", 120, 150);
            delay(3000);
            ESP.restart();
            break;
            
        case HTTP_UPDATE_OK:
            Serial.println("Update SUCCESS!");
            displayOTAStatus("UPDATE", GREEN);
            M5Dial.Display.setTextSize(2);
            M5Dial.Display.drawString("SUCCESS!", 120, 150);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString("REBOOTING...", 120, 180);
            delay(2000);
            ESP.restart();  // This will reboot into the new firmware
            break;
    }
}

// ============================================
// OTA TRIGGER CHECK FUNCTION
// ============================================

bool isOTAUpdateTag(String uuid) {
    // Check if scanned UUID matches any OTA trigger tags
    for (int i = 0; i < OTA_TRIGGER_UIDS_COUNT; i++) {
        if (uuid == OTA_TRIGGER_UIDS[i]) {
            return true;
        }
    }
    return false;
}

// ============================================
// INTEGRATION HELPER
// ============================================
// Add this check to your loop() after M5Dial.Rfid.PICC_ReadCardSerial():
//
// if (M5Dial.Rfid.PICC_IsNewCardPresent() && M5Dial.Rfid.PICC_ReadCardSerial()) {
//     String scannedUuid = sanitizeUuid(readNFCCardUID());
//     if (isOTAUpdateTag(scannedUuid)) {
//         triggerGitHubUpdate();
//     }
//     // ... rest of your RFID logic
// }

#endif // OTA_UPDATE_H
