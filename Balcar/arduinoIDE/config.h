#pragma once

struct ImagesConfig {
    int battery_X;
    int battery_Y;
    int battery_Width;
    int battery_Height;
};

struct DisplayConfig {
    int brightness;
    bool darkMode;
};

struct WifiConfig {
    const char* ssid;
    const char* password;
};

struct Config {
    DisplayConfig display;
};

static Config config = {
    .display = {
        .brightness = 1,
        .darkMode = true
    }
};