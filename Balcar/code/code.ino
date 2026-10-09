#include <M5StickCPlus2.h>
#include <BleKeyboard.h>

#define USE_NIMBLE

BleKeyboard bleKeyboard("gg", "dd", 100);

bool running = false;
bool pressedA = false;
bool pressedS = false;
bool pressedD = false;
bool pressedW = false;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    bleKeyboard.begin();
    bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());

    Serial.begin(115200);

    Serial.println("M5Stick BLE keyboard started");
    M5.Display.sleep();
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
        /*
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
        Serial.print("X: ");
        Serial.print(String((int)fgx, 10));
        Serial.print(" Y: ");
        Serial.print(String((int)fgy, 10));
        Serial.print(" Z: ");
        Serial.print(String((int)fgz, 10));
        Serial.println();*/
        /*
        //... Somewhere, I learned that if first statement is true, then other condition is not checked...but only if it's OR condition
        if (!pressedA && isBetween(percent_X, 70, 100) && isBetween(percent_Z, 0, 50)) {
            //bleKeyboard.press('a');
            pressedA = true;
        } else {
            //bleKeyboard.release('a');
            pressedA = false;
        }
        if (!pressedD && isBetween(percent_X, 30, 80) && isBetween(percent_Z, 70, 100)) {
            //bleKeyboard.press('d');
            pressedD = true;
        } else {
            //bleKeyboard.release('d');
            pressedD = false;
        }
        //... I think that this if-else statement is not needed
        if (!pressedW && isBetween(percent_Y, 40, 65)) {
            //bleKeyboard.press('w');
            pressedW = true;
        } else {
            //bleKeyboard.release('w');
            pressedW = false;
        }
        if (isBetween(percent_Y, 66, 100) && isBetween(percent_Z, 50, 100)) {
            //bleKeyboard.releaseAll();
            pressedA = false;
            pressedD = false;
            pressedS = false;
            pressedW = false;
            return;
        }
        if (!pressedS && isBetween(percent_Y, 50, 100) && isBetween(percent_Z, 0, 50)) {
            //bleKeyboard.press('s');
            pressedS = true;
        } else {
            //bleKeyboard.release('s');
            pressedS = false;
        }*/
        if (fgy > 50 && isBetween(percent_X, 30, 80) && isBetween(percent_Z, 70, 100)) {
            bleKeyboard.release('a');
            bleKeyboard.press('d');
            //Serial.print("d");
        } else if (fgy < -50 && isBetween(percent_X, 70, 100) && isBetween(percent_Z, 0, 50)) {
            bleKeyboard.release('d');
            bleKeyboard.press('a');
            //Serial.print("a");
        } else if (isBetween(percent_X, 80, 100)) {
            bleKeyboard.releaseAll();
            //Serial.print("n");
        }
        delay(75);
    }
}

bool isBetween(float base, float range1, float range2) {
    return base > range1 && base < range2;
}

int toPercent(float value) {
    return (asin(constrain(value, -1.0f, 1.0f)) * 180.0f / PI + 90.0f) / 180.0f * 100.0f;
}
/*
# Akcelerometr
Poloha ruky / Sticku	 X    Y     Z
Střed – volant rovně     ~100 ~50   ~50
Volant doleva            ~50  ~50   ~0
Volant doprava           ~50  ~50   ~100
Dopředu                  ~50  ~50   ~100
Dozadu                   ~50  ~100  ~50
# Gyroskop

*/


