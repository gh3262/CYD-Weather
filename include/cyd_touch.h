#ifndef CYD_TOUCH_H
#define CYD_TOUCH_H

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

#include "cyd_pins.h"
#include "cyd_theme.h"
#include "audio/sound_manager.h"

// ====================================================
// Touch Object
// ====================================================

extern XPT2046_Touchscreen touch;

const int TOUCH_CALIBRATION_X_LEFT = 24;
const int TOUCH_CALIBRATION_X_RIGHT = 296;
const int TOUCH_CALIBRATION_Y_TOP = 72;
const int TOUCH_CALIBRATION_Y_BOTTOM = 174;

struct TouchCalibration
{
    int16_t xLeft;
    int16_t xRight;
    int16_t yTop;
    int16_t yBottom;
};

inline TouchCalibration& touchCalibration()
{
    static TouchCalibration calibration = {
        TOUCH_X_MIN,
        TOUCH_X_MAX,
        TOUCH_Y_MIN,
        TOUCH_Y_MAX
    };
    return calibration;
}

inline bool& touchCalibrationActive()
{
    static bool active = false;
    return active;
}

inline bool isValidTouchCalibration(const TouchCalibration &calibration)
{
    return abs(calibration.xRight - calibration.xLeft) > 500 &&
           abs(calibration.yBottom - calibration.yTop) > 500;
}

// ====================================================
// Initialization
// ====================================================

inline bool initTouch()
{
    SPI.begin(
        TOUCH_SCK,
        TOUCH_MISO,
        TOUCH_MOSI);

    touch.begin();

    Preferences preferences;
    if (!preferences.begin("cyd-touch", false))
    {
        Serial.println("Touch: could not open calibration storage.");
        return true;
    }

    TouchCalibration savedCalibration;
    const size_t savedLength = preferences.getBytesLength("calibration");
    if (savedLength == sizeof(savedCalibration))
    {
        const size_t read = preferences.getBytes(
            "calibration",
            &savedCalibration,
            sizeof(savedCalibration));
        if (read == sizeof(savedCalibration) &&
            isValidTouchCalibration(savedCalibration))
        {
            touchCalibration() = savedCalibration;
            touchCalibrationActive() = true;
        }
        else
        {
            Serial.println("Touch: stored calibration is invalid; using defaults.");
        }
    }
    else if (savedLength != 0)
        Serial.println("Touch: stored calibration has an invalid size; using defaults.");

    preferences.end();

    return true;
}

inline bool saveTouchCalibration(const TouchCalibration &calibration)
{
    if (!isValidTouchCalibration(calibration))
        return false;

    Preferences preferences;
    if (!preferences.begin("cyd-touch", false))
    {
        Serial.println("Touch: could not open calibration storage.");
        return false;
    }

    const size_t written = preferences.putBytes(
        "calibration",
        &calibration,
        sizeof(calibration));
    preferences.end();

    if (written != sizeof(calibration))
    {
        Serial.println("Touch: failed to save calibration.");
        return false;
    }

    touchCalibration() = calibration;
    touchCalibrationActive() = true;
    return true;
}

// ====================================================
// Raw Touch Access
// ====================================================

inline TS_Point getRawTouch()
{
    return touch.getPoint();
}

// ====================================================
// Screen Coordinate Conversion
// ====================================================

inline int rawToScreenX(int rawX)
{
    const TouchCalibration &calibration = touchCalibration();
    return constrain(
        map(rawX,
            calibration.xLeft,
            calibration.xRight,
            touchCalibrationActive() ? TOUCH_CALIBRATION_X_LEFT : 0,
            touchCalibrationActive() ? TOUCH_CALIBRATION_X_RIGHT : SCREEN_WIDTH - 1),
        0,
        SCREEN_WIDTH - 1);
}

inline int rawToScreenY(int rawY)
{
    const TouchCalibration &calibration = touchCalibration();
    return constrain(
        map(rawY,
            calibration.yTop,
            calibration.yBottom,
            touchCalibrationActive() ? TOUCH_CALIBRATION_Y_TOP : 0,
            touchCalibrationActive() ? TOUCH_CALIBRATION_Y_BOTTOM : SCREEN_HEIGHT - 1),
        0,
        SCREEN_HEIGHT - 1);
}

// ====================================================
// Touch Position
// ====================================================

inline int getTouchX()
{
    TS_Point p = touch.getPoint();
    return rawToScreenX(p.x);
}

inline int getTouchY()
{
    TS_Point p = touch.getPoint();
    return rawToScreenY(p.y);
}

inline bool getTouch(int &x, int &y)
{
    TS_Point p = touch.getPoint();

    if (p.z < TOUCH_MIN_PRESSURE)
        return false;

    x = rawToScreenX(p.x);
    y = rawToScreenY(p.y);

    return true;
}

// ====================================================
// Touch Pressure
// ====================================================

inline int getTouchPressure()
{
    return touch.getPoint().z;
}

// ====================================================
// Button Helper
// ====================================================

inline bool touchInRect(
    int touchX,
    int touchY,
    int x,
    int y,
    int w,
    int h)
{
    return (
        touchX >= x &&
        touchX <= (x + w) &&
        touchY >= y &&
        touchY <= (y + h));
}

// Hit test for a tappable button; beeps when the touch lands on it. Check any
// other "is this button active?" condition first so inactive buttons stay silent.
inline bool buttonPressed(
    int touchX,
    int touchY,
    int x,
    int y,
    int w,
    int h)
{
    if (!touchInRect(touchX, touchY, x, y, w, h))
        return false;

    playButtonBeep();
    return true;
}

// ====================================================
// Navigation Buttons
// ====================================================

inline bool navPressed1(int x, int y)
{
    return buttonPressed(
        x,
        y,
        NAV_X1,
        NAV_Y,
        NAV_WIDTH,
        NAV_HEIGHT);
}

inline bool navPressed2(int x, int y)
{
    return buttonPressed(
        x,
        y,
        NAV_X2,
        NAV_Y,
        NAV_WIDTH,
        NAV_HEIGHT);
}

inline bool navPressed3(int x, int y)
{
    return buttonPressed(
        x,
        y,
        NAV_X3,
        NAV_Y,
        NAV_WIDTH,
        NAV_HEIGHT);
}

inline bool navPressed4(int x, int y)
{
    return buttonPressed(
        x,
        y,
        NAV_X4,
        NAV_Y,
        NAV_WIDTH,
        NAV_HEIGHT);
}

#endif