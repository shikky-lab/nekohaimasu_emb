#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H 

#include <NeoPixelBus.h>
#include <M5Unified.h>
#include <main.h>

extern const uint8_t LED_DATA_PIN;
#define NUM_LEDS 16

extern NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip;
const uint8_t INTENSITY=127;
extern const int NUM_OF_COLORS;
enum ColorName {
    Black, White, Red, Lime, Blue, 
    Yellow, Cyan, Magenta, Silver, Gray, 
    Maroon, Olive, Green, Purple, Teal, Navy,
    ColorCount // 色の数を示す特別な値
};
extern const RgbColor predefinedColors[];
void setLeftLEDS(RgbColor color);

void setRightLEDS(RgbColor color);

void setAllLEDs(RgbColor color);

bool showAppearanceTransition_A(uint32_t passedTime);

bool showAppearanceTransition_B(uint32_t currentTime);

bool showAppearanceTransition(uint32_t currentTime);

void showCatPosition(CatState catState,int catPosition);
#endif