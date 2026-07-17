#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_spiffs.h>
#include "secrets.h"

class Logger {
private:
    const char* LOG_FILE = "/mission_log.txt";
    const int MAX_LOG_SIZE = 50000; // ~50KB max
    bool initialized = false;

public:
    void begin() {
        // M5Dial.begin() mounts SPIFFS internally via esp_vfs_spiffs_register().
        // Calling SPIFFS.begin() again would trigger a harmless ESP-IDF
        // "mount failed" error log. Skip if already mounted.
        if (esp_spiffs_mounted(NULL)) {
            initialized = true;
            Serial.println("Logger initialized - SPIFFS already mounted");
            return;
        }

        // SPIFFS not yet mounted — try up to 3 times without formatting.
        for (int attempt = 1; attempt <= 3; attempt++) {
            if (SPIFFS.begin(false)) {
                initialized = true;
                Serial.println("Logger initialized - SPIFFS mounted");
                return;
            }
            Serial.println("SPIFFS mount attempt " + String(attempt) + " failed, retrying...");
            delay(200);
        }

#if SPIFFS_AUTOFIX_ON_MOUNT_FAIL
        Serial.println("SPIFFS mount retries failed - attempting one-time format/remount...");
        if (SPIFFS.begin(true)) {
            initialized = true;
            Serial.println("Logger initialized - SPIFFS mounted after format");
            return;
        }
#endif

        Serial.println("SPIFFS Mount Failed - logging disabled");
    }

    void log(const String& message) {
        if (!initialized) return;

        // Append timestamp and message to log file
        File file = SPIFFS.open(LOG_FILE, "a");
        if (!file) {
            Serial.println("Failed to open log file for writing");
            return;
        }

        // Get elapsed time in ms (rough timestamp)
        unsigned long ms = millis();
        unsigned long secs = ms / 1000;
        unsigned long mins = secs / 60;
        unsigned long hrs = mins / 60;

        // Format: [HH:MM:SS.ms] message
        char timestamp[16];
        snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu]", 
                 hrs % 24, mins % 60, secs % 60);

        file.print(timestamp);
        file.print(" ");
        file.println(message);
        file.close();

        // Check file size and truncate if too large
        checkLogSize();
    }

    void checkLogSize() {
        File file = SPIFFS.open(LOG_FILE, "r");
        if (!file) return;

        size_t size = file.size();
        file.close();

        if (size > MAX_LOG_SIZE) {
            // Truncate: keep only last 25KB
            file = SPIFFS.open(LOG_FILE, "r");
            if (!file) return;

            // Read the last portion
            file.seek(size - 25000);
            String content = file.readString();
            file.close();

            // Rewrite with truncated content
            file = SPIFFS.open(LOG_FILE, "w");
            file.print(content);
            file.close();

            Serial.println("Log file truncated");
        }
    }

    String readLog() {
        if (!initialized) return "Logger not initialized";

        File file = SPIFFS.open(LOG_FILE, "r");
        if (!file) {
            return "No log file found";
        }

        String content = file.readString();
        file.close();

        return content.length() > 0 ? content : "Log is empty";
    }

    // Returns only the last maxBytes of the log to limit heap usage (M3).
    String readLogTail(size_t maxBytes = 2048) {
        if (!initialized) return "Logger not initialized";
        File file = SPIFFS.open(LOG_FILE, "r");
        if (!file) return "No log file found";
        if (file.size() > maxBytes) {
            file.seek(file.size() - maxBytes);
            file.readStringUntil('\n');  // discard partial leading line
        }
        String content = file.readString();
        file.close();
        return content.length() > 0 ? content : "Log is empty";
    }

    void clearLog() {
        if (!initialized) return;

        if (SPIFFS.remove(LOG_FILE)) {
            Serial.println("Log file cleared");
        }
    }

    int getLogLineCount() {
        if (!initialized) return 0;

        File file = SPIFFS.open(LOG_FILE, "r");
        if (!file) return 0;

        int lineCount = 0;
        while (file.available()) {
            if (file.read() == '\n') {
                lineCount++;
            }
        }
        file.close();
        return lineCount;
    }
};
