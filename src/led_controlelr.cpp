
#include <NeoPixelBus.h>
#include "led_contoller.h"
#include  "main.h"

NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(NUM_LEDS, LED_DATA_PIN);
const RgbColor predefinedColors[] = {
    RgbColor(0, 0, 0),       // Black
    RgbColor(INTENSITY, INTENSITY, INTENSITY), // White
    RgbColor(INTENSITY, 0, 0),     // Red
    RgbColor(0, INTENSITY, 0),     // Lime
    RgbColor(0, 0, INTENSITY),     // Blue
    RgbColor(INTENSITY, INTENSITY, 0),   // Yellow
    RgbColor(0, INTENSITY, INTENSITY),   // Cyan / Aqua
    RgbColor(INTENSITY, 0, INTENSITY),   // Magenta / Fuchsia
};
constexpr int NUM_OF_COLORS = sizeof(predefinedColors) / sizeof(predefinedColors[0]);

void setLeftLEDS(RgbColor color){
  for(int i=0;i<NUM_LEDS/2;i++){
    strip.SetPixelColor(i,color);
  }
  strip.Show();
  return;
}

void setRightLEDS(RgbColor color){
  for(int i=0;i<NUM_LEDS/2;i++){
    strip.SetPixelColor(NUM_LEDS-i,color);
  }
  strip.Show();
  return;
}

void setAllLEDs(RgbColor color){
  for(int i=0;i<NUM_LEDS;i++){
    strip.SetPixelColor(i,color);
  }
  strip.Show();
  return;
}

/*
両端から一つずつ点灯して中央へ
*/
bool showAppearanceTransition_A(uint32_t passedTime){
  const int TOTAL_TIME=2000;

  RgbColor color=predefinedColors[White];
  bool ledValues[NUM_LEDS] ={0};

  int phase_duration = TOTAL_TIME/(NUM_LEDS/2);
  for(int i=0;i<NUM_LEDS/2;i++){
    if(passedTime > phase_duration*i){
      ledValues[i]=ledValues[NUM_LEDS-1-i]=true;
    }
  }

  for(int i=0;i<NUM_LEDS;i++){
    if(ledValues[i]){
      strip.SetPixelColor(i,color);
    }else{
      strip.SetPixelColor(i,predefinedColors[Black]);
    }
  }
  strip.Show();

  return passedTime>TOTAL_TIME;
}

/*
ITERATION回数だけランダムに点灯したのち、1秒間暗転からの、一秒間発光
*/
bool showAppearanceTransition_B(uint32_t currentTime){
  const int TOTAL_TIME=2000;
  const int ITERATION=10;
  const int EACH_FLASH_DURATION=100;//ms
  const int DARK_DURATION=1000;//ms
  const int LAST_FLASH_DURATION=1000;//ms
  int phase_duration = TOTAL_TIME/ITERATION;
  static RgbColor color = 0;
  static uint32_t next_rumble_time=0;
  static uint8_t count=0;
  static bool finishFlag=false;

  if(next_rumble_time<currentTime){
    if(count<ITERATION){
        color = predefinedColors[random(1,NUM_OF_COLORS-1)];//黒以外
        next_rumble_time = currentTime+EACH_FLASH_DURATION;
        count++;
    }else if(count == ITERATION){
        color = predefinedColors[Black];//黒
        next_rumble_time = currentTime+DARK_DURATION;
        count++;
    }else if(count == ITERATION+1){
        color = predefinedColors[Yellow];
        next_rumble_time = currentTime+LAST_FLASH_DURATION;
        count++;
    }else{
      color = predefinedColors[Black];
      finishFlag=true;
    }
  }
  setAllLEDs(color);

  if(finishFlag){
    next_rumble_time=0;
    count=0;
    finishFlag=false;
    return true;
  }

  return false;
}

bool showAppearanceTransition(uint32_t currentTime){
  static uint32_t firstCalledTime = 0;
  static bool finished_A = false;
  static bool finished_B = false;
  if(firstCalledTime == 0 ){
    firstCalledTime = currentTime;
  }
  if(!finished_A){
    uint32_t passedTime = currentTime - firstCalledTime;
    finished_A = showAppearanceTransition_A(passedTime);
  }else if(!finished_B){
    finished_B = showAppearanceTransition_B(currentTime);
  }

  if(finished_B){
    firstCalledTime = 0;
    finished_A = false;
    finished_B = false;
    return true;
  }

  return false;
}

void showCatPosition(CatState catState,int catPosition){
  if(catState != CatState::EXIST){
    return;
  }

  int led_index= map(catPosition,-100,100,0,NUM_LEDS);
  
  RgbColor color =predefinedColors[Yellow];
  for(int i=0;i<NUM_LEDS;i++){
    if(led_index-1 <= i && i <= led_index + 1 ){
      strip.SetPixelColor(i,color);
    }else{
      strip.SetPixelColor(i,predefinedColors[Black]);
    }
  }
  strip.Show();
}