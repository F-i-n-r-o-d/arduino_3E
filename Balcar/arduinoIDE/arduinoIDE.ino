#include <M5StickCPlus2.h>
//#include <WiFi.h>
#include "pictures.h"
#include "config.h"
#include "cube.h"
#include <IRremote.h>
#include <BleKeyboard.h>

/*
#include <EEPROM.h>
#define EEPROM_SIZE 64
#if defined(USE_EEPROM)
    EEPROM.write(3, region);
    EEPROM.commit();
#endif
*/

bool irSetuped = false;
bool runIR = false;
bool uspavat = true;
bool showHidden = false;
bool clearShowHidden = true;
bool debug = true;
bool clear = true;
bool showCopyright = true;
bool sleeping = false;
// 1 = hodiny; 2 = wifi; 3 = bluetooth; 4 = IR
int renderIndex = 1;
int renderedIndex;
int choosen = 0;
int prevMinute = 99;
String dnyTydnu[] = {"NE", "PO", "UT", "ST", "CT", "PA", "SO"};
int brightness = 1;

int lastActionTime = 0;
// buttons; 0 = nothing (default); 1 = was pressed in last actionTime; 2 = is currently pressed
// main front button
int lastBtnAState = 0;
// side navigation button
int lastBtnBState = 0;
// power button
int lastBtnPwrState = 0;


// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR
// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR
// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR
// -=-=- FEATURES -=-=-
#define ACTIVE_LOW_IR
#define M5LED 19
#define ROTATION
// -=-=- ALIASES -=-=-
#define IRLED 19
#define M5_BUTTON_MENU 35
#define M5_BUTTON_HOME 37
#define M5_BUTTON_RST 39
#define BACKLIGHT 27
#define MINBRIGHT 190
#define M5LED_ON HIGH
#define M5LED_OFF LOW

#include "WORLD_IR_CODES.h"
/// TV-B-GONE ///

bool check_select_press() {
    return digitalRead(39) == LOW;
    //return M5.BtnB.isPressed();
}

void tvbgone_setup() {
    if (irSetuped) {
        return;
    }
    IrSender.begin(IRLED); // Initialize IR sender
    // Hack: Set IRLED high to turn it off after setup. Otherwise it stays on (active low)
    digitalWrite(IRLED, M5LED_OFF);
    delay_ten_us(5000);
    //M5.Display.setTextSize(0.2);
    //M5.Display.println("EU");
    delay(1000);
    region = 1;
    irSetuped = true;
}

void tvbgone_loop() {
    delay(250);
    sendAllCodes();
}

void sendAllCodes() {
    bool endingEarly = false; //will be set to true if the user presses the button during code-sending
    if (region == NA) {
        num_codes = num_NAcodes;
    } else {
        num_codes = num_EUcodes;
    }
    //num_codes = num_EUcodes;
    for (i = 0 ; i < num_codes; i++) {
        if (region == NA) {
            powerCode = NApowerCodes[i];
        }
        else {
            powerCode = EUpowerCodes[i];
        }
        const uint8_t freq = powerCode->timer_val;
        const uint8_t numpairs = powerCode->numpairs;
        const uint8_t bitcompression = powerCode->bitcompression;
        code_ptr = 0;
        for (uint8_t k = 0; k < numpairs; k++) {
            uint16_t ti;
            ti = (read_bits(bitcompression)) * 2;
            #if defined(ACTIVE_LOW_IR)
                offtime = powerCode->times[ti];  // read word 1 - ontime
                ontime = powerCode->times[ti + 1]; // read word 2 - offtime
            #else
                ontime = powerCode->times[ti];  // read word 1 - ontime
                offtime = powerCode->times[ti + 1]; // read word 2 - offtime      
            #endif
            //M5.Display.setTextSize(2);
            //M5.Display.printf("rti = %d Pair = %d, %d\n", ti >> 1, ontime, offtime);
            //Serial.printf("TVBG: rti = %d Pair = %d, %d\n", ti >> 1, ontime, offtime);
            rawData[k * 2] = offtime * 10;
            rawData[(k * 2) + 1] = ontime * 10;
        }
        IrSender.sendRaw(rawData, (numpairs * 2), freq);
        digitalWrite(IRLED, M5LED_OFF);
        bitsleft_r = 0;
        delay_ten_us(20500);
        if (check_select_press()){
            endingEarly = true;
            runIR = false;
            delay(250);
            break; 
        }
    }
    if (endingEarly == false) {
        delay_ten_us(MAX_WAIT_TIME); // wait 655.350ms
        delay_ten_us(MAX_WAIT_TIME); // wait 655.350ms
        quickflashLEDx(8);
    }
}
// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR
// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR
// IRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIRIR


void pass() {}

void setup() {
    auto cfg = M5.config();
    // abych nekomunikoval s PC
    cfg.serial_baudrate = 0;
    cfg.output_power = false;
    cfg.clear_display = true;
    cfg.disable_rtc_irq = true;
    cfg.led_brightness = 0;
    M5.begin(cfg);
    M5.Power.begin();
    M5.Power.setUsbOutput(false);

    M5.Display.setRotation(3);
    M5.Display.setBrightness(1);
    //attachInterrupt(digitalPinToInterrupt(M5.BtnA), handlePress);
    // vypnout wifi a bluetooth
    //WiFi.disconnect(true);
    //WiFi.mode(WIFI_OFF);
    btStop();
    setCpuFrequencyMhz(debug ? 20 : 240);
    M5.Display.powerSave(true);
    M5.Imu.begin();
    Serial.begin(115200);
}

void loop() {
    M5.update();

    /* ještě vymyslet, jestli funkci nebo porovnání
    bude si to muset pamatovat poslední stav a podle něj porovnat
    if (M5.BtnA.getState() == M5.BtnA.state_clicked) {M5.BtnA.isPressed();}
    enum button_state_t : std::uint8_t
    { state_nochange
    , state_clicked
    , state_hold
    , state_decide_click_count
    }; */

    if (M5.BtnA.isPressed() && (millis() - lastActionTime > 150 || (showHidden && millis() - lastActionTime > 20))) {
        sleeping ? wakeup() : select();
        lastActionTime = millis();
    } else if (M5.BtnPWR.isPressed() && millis() - lastActionTime > 150) {
        sleeping ? wakeup() : prev();
        lastActionTime = millis();
    } else if (M5.BtnB.isPressed() && millis() - lastActionTime > 150) {
        sleeping ? wakeup() : next();
        lastActionTime = millis();
    }
    if ((millis() - lastActionTime > 6000) && uspavat) {
        sleep();
    }

    render();
    delay(20);
}

void sleep() {
    sleeping = true;
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    M5.Power.lightSleep(pow(10, 14));
}

void wakeup() {
    sleeping = false;
    M5.Display.wakeup();
    render();
    M5.Display.setBrightness(brightness);
    delay(500);
    //... better change it in loop so that it will not call next() function
    //sleeping = false;
}

void select() {
    if (showHidden) {
        brightness++;
        if (brightness == 256) {
            brightness = 1;
        }
        //brightness = pow(brightness/255.0, 2.2)*255;
        M5.Display.setBrightness(brightness);
        drawBrightness();
    }
    if (renderedIndex == 1 || renderedIndex == 0) {
        if (showHidden) {
            renderHidden();
            return;
        }
        sleep();
    } else if (renderedIndex == 2) {
        renderIndex = 20;
        clear = true;
    } else if (renderedIndex == 20) {
        //configTimeThroughtWifi();
    } else if (renderedIndex == 3) {
    } else if (renderedIndex == 4) {
        renderIndex = 40;
        runIR = true;
    } else if (renderedIndex == 5) {
        //sleeping = true;
        //M5.Display.setBrightness(0);
        //M5.Display.sleep();
        //M5.Power.deepSleep(pow(10, 14));
        bleKeyboardLoop();
    }
}

void next() {
    if (showHidden) {
        renderIndex = 0;
        showHidden = false;
        clearShowHidden = true;
    }
    clear = true;
    renderIndex++;
    if (renderIndex == 6) {
        renderIndex = 5;
        clear = false;
    }
    /*if (choosen > 1) {
        if (choosen == 2) {
            renderIndex = 20;
        } else if (choosen == 3) {
            renderIndex = 30;
        }
    }*/
}

void prev() {
    clear = true;
    renderIndex--;
    if (renderIndex == 0) {
        showHidden = true;
        renderIndex = 1;
        clear = false;
    }
    if ((renderedIndex - 20) == 0) {
        renderIndex = 2;
        clear = true;
    }
}

void renderHidden() {
    if (clearShowHidden) {
        clearShowHidden = false;
        M5.Display.fillScreen(0x0);
        M5.Display.pushImage(0, 0, 240, 135, image_fotka_hidden);
    }
}

void renderMain() {
    if (showHidden) {
        return;
    }
    auto time = M5.Rtc.getTime();
    auto date = M5.Rtc.getDate();
    M5.Display.startWrite();
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    if (clear) {
        M5.Display.fillScreen(TFT_BLACK);
        // line
        M5.Display.drawLine(64, 80, 174, 80, TFT_GREEN);
        // rect
        M5.Display.drawRect(63, 79, 113, 3, 0x0400);
        // datum
        M5.Display.setTextSize(1);
        M5.Display.setCursor(50, 85);
        M5.Display.print(dnyTydnu[date.weekDay] + " " + (String) date.date +
                         "." + (String) date.month + "." + (String) date.year);
        drawArrow(2);
    }
    if (prevMinute != time.minutes || clear) {
        M5.Display.fillRect(33, 21, 174, 55, TFT_BLACK);
        M5.Display.setTextSize(3);
        M5.Display.setCursor(33, 21);
        String hours;
        String minutes;
        if ((time.hours) % 24 < 10) {
            hours = "0" + (String) ((time.hours) % 24);
        } else hours = (String) ((time.hours) % 24);
        if (time.minutes < 10) {
            minutes = "0" + (String) time.minutes;
        } else minutes = (String) time.minutes;
        M5.Display.print(hours + ":" + minutes);
        prevMinute = time.minutes;
        drawBattery();
    }
    if (clear && showCopyright) {

        // podpis
        M5.Display.setTextSize(0.5);
        // při velikosti 0.5 je pozice [100;122]
        // při velikosti 1 je pozice [53;110]
        M5.Display.setCursor(95, 122);
        M5.Display.print("by danb1551");
    }
    M5.Display.endWrite();
    if (clear) {
        delay(400);
    }
}

void renderWifi() {
    if (clear) {
        M5.Display.fillScreen(TFT_BLACK);
        drawArrow(1);
        drawArrow(2);
        M5.Display.drawBitmap(51, 6, image_wifi_logo, 139, 99, TFT_GREEN);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.setCursor(90, 112);
        M5.Display.print("Wi-Fi");
        delay(400);
    }
}

void renderWifiMenu(int index) {
    M5.Display.startWrite();
    if (clear) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.drawBitmap(51, 6, image_wifi_logo, 139, 99, TFT_GREEN);
        clear = false;
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.setCursor(90, 112);
    drawArrow(2);
    if (index == 0) {
        M5.Display.print("Wi-Fi");
        renderedIndex = index + 20;
    }
    M5.Display.endWrite();
    delay(400);
}

void renderBluetooth() {
    if (clear) {
        M5.Display.fillScreen(TFT_BLACK);
        drawArrow(1);
        drawArrow(2);
        M5.Display.fillRect(102, 30, 28, 52, TFT_WHITE);
        M5.Display.drawBitmap(92, 24, image_bluetooth_logo, 48, 64, TFT_BLUE);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.setCursor(70, 112);
        M5.Display.print("Bluetooth");
        delay(400);
    }
}

void renderBluetoothMenu(int index) {
    if (index == 1) {
        return;
    }
}

void renderIR() {
    if (clear) {
        M5.Display.fillScreen(TFT_BLACK);
        drawArrow(1);
        drawArrow(2);
        M5.Display.drawBitmap(94, 25, image_ir_signal, 49, 23, TFT_RED);
        M5.Display.fillRoundRect(102, 51, 36, 45, 12, 0xBDF7);
        M5.Display.fillEllipse(113, 68, 5, 5, 0x0);
        M5.Display.fillEllipse(126, 68, 5, 5, 0x0);
        M5.Display.fillEllipse(113, 80, 5, 5, 0x0);
        M5.Display.fillEllipse(126, 80, 5, 5, 0x0);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.setCursor(80, 112);
        M5.Display.print("InfraRed");
        if (!runIR) {
            M5.Display.setTextSize(1);
            M5.Display.setCursor(0, 0);
            M5.Display.print("S");
        }
        delay(400);
    }
}

// this will render and at the same time start sending codes
void renderIRMenu() {
    if (runIR) {
        uspavat = false;
        M5.Display.fillRect(0, 0, 25, 50, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(0, 0);
        M5.Display.print("P");
        tvbgone_setup();
        tvbgone_loop();
    } else {
        uspavat = true;
    }
    renderIndex = 4;
    clear = true;
}

void renderImu() {
    drawCubeIMU();
    delay(100);
}

    /// bitmaps (1 = left; 2 = right)
void drawArrow(int combination) {
    M5.Display.startWrite();
    if (combination == 1) {
        M5.Display.drawBitmap(4, 116, image_arrow_left, 24, 15, TFT_GREEN);
    } else if (combination == 2) {
        M5.Display.drawBitmap(212, 116, image_arrow_right, 24, 15, TFT_GREEN);
    }
    M5.Display.endWrite();
}

void clearBatteryPlace() {
    M5.Display.fillRect(224, 10, 10, 14, TFT_BLACK);
}

void drawBattery() {
    int batteryLevel = M5.Power.getBatteryLevel();
    M5.Display.startWrite();
    //... no, I will not explain it
    int color = batteryLevel > 50 ? TFT_GREEN : (batteryLevel > 25 ? TFT_YELLOW : TFT_RED);

    clearBatteryPlace();
    M5.Display.drawBitmap(224, 8, image_baterry_case, 10, 14, TFT_GREEN);
    if (batteryLevel > 25) M5.Display.fillRect(226, 18, 6, 2, color);
    if (batteryLevel > 50) M5.Display.fillRect(226, 15, 6, 2, color);
    if (batteryLevel > 75) M5.Display.fillRect(226, 12, 6, 2, color);
    M5.Display.endWrite();
}

void render() {
    /*if (choosen > 1) {
        if (choosen == 2) {
            renderWifiMenu(1);
        } else if (choosen == 3) {
            renderBluetoothMenu(1);
        }
    }*/
    if (showHidden) {
        M5.Display.powerSave(false);
        renderHidden();
        uspavat = false;
    } else {
        M5.Display.powerSave(true);
        uspavat = true;
    }

    if (renderIndex == 0 || renderIndex == 1) {
        renderMain();
        renderedIndex = 1;
    } // 2222222222222222222222222
      else if (renderIndex == 2) {
        renderWifi();
        renderedIndex = 2;
    } else if (renderIndex >= 20 && renderIndex < 30) {
        renderWifiMenu(renderIndex - 20);
    } else if (renderIndex == 3) {
        renderBluetooth();
        renderedIndex = 3;
    } // 4444444444444444444444444
      else if (renderIndex == 4) {
        renderIR();
        renderedIndex = 4;
    } else if (renderIndex == 40) {
        renderIRMenu();
    } // 5555555555555555555555555
      else if (renderIndex == 5) {
        renderImu();
        uspavat = false;
        renderedIndex = 5;
    }
    clear = false;
}

void drawBrightness() {
    M5.Display.startWrite();
    if (brightness == 1) {
        clearShowHidden = true;
        renderHidden();
    }
    M5.Display.drawRoundRect(25, 109, 190, 13, 4, 0x0);
    M5.Display.drawRoundRect(26, 110, 188, 11, 3, 0x0);
    int width = map(brightness, 1, 255, 0, 186);
    M5.Display.fillRoundRect(27, 111, width, 9, 3, TFT_GREEN);
    Serial.println(width);
    M5.Display.endWrite();
}

/*void configTimeThroughtWifi() {
    WiFi.begin("a");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    const long gmtOffset_sec = 3600; 
    const int daylightOffset_sec = 3600;
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println(&timeinfo, "Time synchronized: %A, %B %d %Y %H:%M:%S");
    } else {
        Serial.println("Failed to obtain time");
    }
}*/

BleKeyboard bleKeyboard("gg", "dd", 100);
void bleKeyboardLoop() {
    M5.Display.sleep();
    bleKeyboard.begin();
    bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());
    bool run = true;
    while (run) {
        M5.update();
        if (M5.BtnA.isPressed()) {
            run = !run;
            continue;
        }
        if (bleKeyboard.isConnected()) {
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
            if (fgy > 50 && isBetween(percent_X, 30, 80) && isBetween(percent_Z, 70, 100)) {
                bleKeyboard.release('a');
                bleKeyboard.press('d');
            } else if (fgy < -50 && isBetween(percent_X, 70, 100) && isBetween(percent_Z, 0, 50)) {
                bleKeyboard.release('d');
                bleKeyboard.press('a');
            } else if (isBetween(percent_X, 80, 100)) {
                bleKeyboard.releaseAll();
            }
            delay(75);
        } else {
            delay(75);
        }
    }
    bleKeyboard.releaseAll(); 
    M5.Display.wakeup();
}

bool isBetween(float base, float range1, float range2) {
    return base > range1 && base < range2;
}

int toPercent(float value) {
    return (asin(constrain(value, -1.0f, 1.0f)) * 180.0f / PI + 90.0f) / 180.0f * 100.0f;
}
















