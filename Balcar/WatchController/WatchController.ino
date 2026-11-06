// umi wasd. První verze
#include<M5StickCPlus2.h>
#include<BleKeyboard.h>/*sdkfyswk46657*/
#define DEBUG false
#define DEBUG2 false

BleKeyboard bleKeyboard("gg", "dd", 100);

bool running = false;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.sleep();

    bleKeyboard.begin();
    bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());

#if DEBUG || DEBUG2
    Serial.begin(115200);
    Serial.println("M5Stick BLE keyboard started");
#endif
}

void loop() {
    M5.update();
    if (M5.BtnA.isPressed()) {
        running = !running;
        bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());
        delay(350);
    }
    if(bleKeyboard.isConnected() && running) {
        float fax, fay, faz;
        float fgx, fgy, fgz;
        M5.Imu.getAccel(&fax, &fay, &faz);
        M5.Imu.getGyro(&fgx, &fgy, &fgz);
        int percent_X = toPercent(fax);
        int percent_Y = toPercent(fay);
        int percent_Z = toPercent(faz);
        int percent_GX = toPercent(fgx);
        int percent_GY = toPercent(fgy);
        int percent_GZ = toPercent(fgz);

#if DEBUG
        Serial.print("X: ");
        Serial.print(String(percent_X, 10));
        Serial.print(" Y: ");
        Serial.print(String(percent_Y, 10));
        Serial.print(" Z: ");
        Serial.print(String(percent_Z, 10));
        Serial.print("X: ");
        Serial.print(String(percent_GX, 10));
        Serial.print(" Y: ");
        Serial.print(String(percent_GY, 10));
        Serial.print(" Z: ");
        Serial.print(String(percent_GZ, 10));
        Serial.println();
#else
        if (fgy > 50 && isBetween(percent_X, 30, 80) && isBetween(percent_Z, 70, 100)) {
    #if DEBUG2
            Serial.print("d");
    #else
            bleKeyboard.press('d');
            bleKeyboard.release('a');
    #endif
        } else if (fgy < -50 && isBetween(percent_X, 70, 100) && isBetween(percent_Z, 0, 50)) {
    #if DEBUG2
            Serial.print("a");
    #else
            bleKeyboard.press('a');
            bleKeyboard.release('d');
    #endif
        } else if (isBetween(percent_X, 80, 100)) {
    #if DEBUG2
            Serial.print("\n");
    #else
            bleKeyboard.releaseAll();
    #endif
        }
        if (isBetween(percent_Y, 40, 65)) {
    #if DEBUG2
            Serial.print("w");
    #else
            bleKeyboard.press('w');
            bleKeyboard.release('s');
    #endif
        } else if (isBetween(percent_Y, 66, 75)) {
    #if DEBUG2
            Serial.print("\0");
    #else
            bleKeyboard.release('w');
            bleKeyboard.release('s');
    #endif
        } else if (isBetween(percent_Y, 80, 100)) {
    #if DEBUG2
            Serial.print("s");
    #else
            bleKeyboard.press('s');
            bleKeyboard.release('w');
    #endif
        }
#endif
        delay(75*3);
    }
}

bool isBetween(float base, float range1, float range2) {
    return base > range1 && base < range2;
}

int toPercent(float value) {
    return (asin(constrain(value, -1.0f, 1.0f)) * 180.0f / PI + 90.0f) / 180.0f * 100.0f;
}