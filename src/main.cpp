#include <M5Unified.h>
// #include <FastLED.h>
#include <NeoPixelBus.h>
#include <BluetoothSerial.h>
// #include <WiFi.h>
#include <FS.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "led_contoller.h"

/*
責務：
- コマンドで指定した量だけサーボを前後させる。
- コマンドは改行で区切る。\r,\n,\r\nは問わない：
- サーボ動作中は電磁石をオンにする
- 定期的に現状のセンサ状態を送信する

受信
L数字：左方向に動かした後、戻る。数字は0~100を指定。ex：L70
R数字：右方向に動かした後、戻る。数字は0~100を指定。ex：R30
e:電磁石を一定時間起動する(デバッグ用)
P数字：数字は-100~100で、その位置周辺のLEDを光らせる
v: 猫の登場アクション
V: 猫の退場アクション

送信
L%bR%b\r\n :%bは0/1で、リードスイッチが反応しているときに1を返す
*/

//internal global variable
static BluetoothSerial SerialBT;
static String incomingData = "";

// 押し出し関係の定数定義
const int LIMIT_LENGTH = 30; //ラックを動かせる最大量。mm。。
const int MAX_LENGTH = 30; //ラックを動かす最大量。mm。POWER=100の場合にこの距離を動かす。
// const int PITCH_DIAMETER = 80; // ピッチ円直径。mm

//こちらから送信する情報の頻度
const int HEARTBEAT_INTERVAL = 20;//ms

//サーボ用のPWM関係の変数
const uint32_t PWM_Hz = 50;   // PWM周波数
const uint8_t PWM_level = 16; // PWM分解能 16bit
const uint8_t PWM_CH = 1;  // チャンネル
const uint8_t PWM_PIN=G33;
static unsigned long servo_round_trip_until=0;
static int servo_round_trip_duration=0;
static int servo_destination_pulse=0;
const int servo_interuption_interval=50; //次のサーボ位置計算の頻度。ms

//電磁石関係
const uint8_t emag_pin=G32;
enum class EmugState{
  NEUTORAL,PLUS
};
EmugState emugState=EmugState::NEUTORAL;
unsigned long emag_enable_until=0;

//LEDテープ関係. 実定義は別ファイル。
const uint8_t LED_DATA_PIN=G0;

//猫の状態
CatState catState=CatState::NO_EXIST;
int catPosition = 0;


//リードスイッチ関係
const uint8_t SENSOR_PIN_LEFT=G26;
const uint8_t SENSOR_PIN_RIGHT=G36;
int previous_left_value;
int previous_right_value;

//Wifi,REST関係
// const char *wifi_ssid="aterm-lnld-g";
// const char *wifi_password="7korobi8oki";
// #define HTTP_PORT 80
// AsyncWebServer server(HTTP_PORT);
// #define FORMAT_SPIFFS_IF_FAILED true

//サーボ関係
#define MIDDLLE_DUTY 0.075 
#define DUTY_RANGE 0.025
const int MIDDLE_PULSE=((1<<PWM_level)-1)*MIDDLLE_DUTY;
// const int PULSE_RANGE=((1<<PWM_level)-1)*DUTY_RANGE; // 1msで左端、2msで右端

//prototype declaration
void printLine(String text);
void init_emag();
void change_emag_state(EmugState enumState,int duration=0);
void push(char direction, int power,long servo_duration=0);
bool isNumber(String str);

// void wifi_connect(const char *ssid, const char *password){
//   printLine("WiFi Connenting...");

//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(1000);
//   }

//   printLine("Connected.");
//   printLine("IP:"+WiFi.localIP().toString());
//   // printLine("Gateway IP: "+WiFi.gatewayIP().toString());
// }

// void notFound(AsyncWebServerRequest *request){
//   if (request->method() == HTTP_OPTIONS){
//     request->send(200);
//   }else{
//     request->send(404);
//   }
// }

int cursorX=0;
int cursorY=0;
const int lineHeight=16;
void printLine(String text) {
  // 行が画面の高さを超えたらスクロール
  if (cursorY + lineHeight > M5.Lcd.height()) {
    M5.Lcd.scroll(0, -lineHeight);
    cursorY = M5.Lcd.height() - lineHeight;
    M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), lineHeight, TFT_BLACK); // 新しい行の部分を黒で塗りつぶす
  }
  
  // テキストの表示
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.print(text);
  
  // 次の行へ
  cursorY += lineHeight;
}

void init_emag(){
  pinMode(emag_pin,OUTPUT);
  change_emag_state(EmugState::NEUTORAL);
}

void change_emag_state(EmugState __emugState,int duration){
  emugState=__emugState;
  switch(__emugState){
    case (EmugState::NEUTORAL): 
      digitalWrite(emag_pin,LOW);
      // SerialBT.println("NEUTORAL");
      break;
    case (EmugState::PLUS): 
      digitalWrite(emag_pin,HIGH);
      // SerialBT.println("PLUS");
      break;
  }


  if(duration != 0){
    emag_enable_until =  millis()+duration;
  }
}

void set_servo_push(char direction, int power,long servo_duration=0){
  int sign=1;
  if(direction == 'L'){
    sign=-1;
  }
  if (power>100){
    power=100;
  }

  float duty_rate=DUTY_RANGE*power/100;
  int pulse=((1<<PWM_level)-1)* (MIDDLLE_DUTY+(sign * duty_rate));

  servo_destination_pulse = pulse;
  servo_round_trip_duration=servo_duration;
  servo_round_trip_until = millis() + servo_duration;
}

void push(char direction, int power,long servo_duration){
  set_servo_push(direction, power,servo_duration);
  // change_emag_state(EmugState::PLUS,servo_duration*1.5);
  change_emag_state(EmugState::PLUS,servo_duration);
}


bool isNumber(String str){
  for (int i = 0; i < str.length(); i++) {
    if (!isDigit(str[i])) {
      if(i==0 && '-' == str[i]){
        continue;
      }
      return false; 
    }
  }
  return true;
}

void processReadData(){
  incomingData.trim();
  if(incomingData==""){
    return;
  }

  String copiedIncomingData = incomingData;
  incomingData = ""; // 受信データをクリア

  if (copiedIncomingData.startsWith("L") || copiedIncomingData.startsWith("R")) {
    //L100,2000のような形で、カンマ前がpower, カンマ後がduration
    int power=100;
    int duration=1000;

    // カンマの位置を探す.無ければPowerだけ反映
    int commaIndex = copiedIncomingData.indexOf(',');
    if (commaIndex == -1) {
      String powerString = copiedIncomingData.substring(1); // L,Rを除いた文字列を取得
      if(!isNumber(powerString)){
        printLine("powerString is not number, passed: " + powerString);
        return;
      }

      power = powerString.toInt(); // int型に変換
      duration = 1000;
    }else{
      // パワーの部分を抽出して整数に変換
      String powerString = copiedIncomingData.substring(1, commaIndex);
      power = powerString.toInt();
      if(!isNumber(powerString)){
        printLine("powerString is not number, passed: " + powerString);
        return;
      }

      // カンマの後の部分を抽出して整数に変換
      String durationStr = copiedIncomingData.substring(commaIndex + 1);
      durationStr.trim(); // 改行文字や余分な空白を削除
      if(!isNumber(durationStr)){
        printLine("duration is not number, passed: " + durationStr);
        return;
      }
      duration = durationStr.toInt();
    }

    char direction=copiedIncomingData[0];

    push(direction,power,duration);
    printLine(String(direction)+":P"+String(power)+",D"+String(duration));

  }else if(copiedIncomingData.startsWith("e")){
    change_emag_state(EmugState::PLUS,3000);
  }else if(copiedIncomingData.startsWith("v")){
    catState = CatState::APPEARING;
  }else if(copiedIncomingData.startsWith("V")){
    catState = CatState::NO_EXIST;
    setAllLEDs(predefinedColors[Black]);
  }else if(copiedIncomingData.startsWith("P")){
    String positionString = copiedIncomingData.substring(1); 
    if(!isNumber(positionString)){
      printLine("powerString is not number, passed: " + positionString);
    }
    catPosition = positionString.toInt(); // int型に変換
  }
}

void move_servo(uint32_t current_time){
  if(servo_round_trip_until < current_time){
    ledcWrite(PWM_CH,MIDDLE_PULSE);
    return;
  }

  int rest_time = servo_round_trip_until - current_time;
  float next_target_pos_rate = (float)rest_time/servo_round_trip_duration;

  int next_pulse;
  if(next_target_pos_rate<0.5){//行き
    next_pulse = MIDDLE_PULSE + (servo_destination_pulse-MIDDLE_PULSE)*next_target_pos_rate*2;
  }else{//帰り
    next_pulse = servo_destination_pulse - (servo_destination_pulse-MIDDLE_PULSE)*(next_target_pos_rate-0.5)*2;
  }

  ledcWrite(PWM_CH,next_pulse);

  if(emugState!=EmugState::NEUTORAL){
    // SerialBT.println(String(next_pulse));
  }
}


void sendHeartbeat(int leftValue, int rightValue){
  SerialBT.printf("L%dR%d\r\n",leftValue,rightValue);
}

void checkBoxPosition(){
  bool update_flag=false;

  int current_left_value = digitalRead(SENSOR_PIN_LEFT);
  if(current_left_value != previous_left_value){
    update_flag=true;
  }
  previous_left_value = current_left_value;


  int current_right_value = digitalRead(SENSOR_PIN_RIGHT);
  if(current_right_value != previous_right_value){
    update_flag=true;
  }
  previous_right_value = current_right_value;

  if(update_flag){
    if(catState == CatState::NO_EXIST){
      RgbColor on_color = predefinedColors[Green];
      RgbColor off_color = predefinedColors[Black];
      strip.SetPixelColor(0,current_left_value==LOW?on_color:off_color);
      strip.SetPixelColor(NUM_LEDS-1,current_right_value==LOW?on_color:off_color);
      strip.Show();
    }

    //Active Lowなので反転して送る
    sendHeartbeat(!current_left_value,!current_right_value);
  }
}

void init_reed_switch(){
  pinMode(SENSOR_PIN_LEFT,INPUT_PULLUP);
  pinMode(SENSOR_PIN_RIGHT,INPUT_PULLUP);
  previous_left_value = digitalRead(SENSOR_PIN_LEFT);
  previous_right_value = digitalRead(SENSOR_PIN_RIGHT);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Power.begin();

  M5.Lcd.setRotation(1); //画面回転
  M5.Lcd.fillScreen(TFT_BLACK); // 画面をクリア
  M5.Lcd.setTextFont(2); // フォントの設定
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); // テキストと背景の色を設定

  SerialBT.begin("AR_CAT_STICKC");

  //サーボ関係
  // チャンネルと周波数の分解能を設定
  ledcSetup(PWM_CH, PWM_Hz, PWM_level);
  // モータのピンとチャンネルの設定
  ledcAttachPin(PWM_PIN, PWM_CH);
  ledcWrite(PWM_CH,MIDDLE_PULSE);

  //電磁石関係
  init_emag();

  //センサー制御
  init_reed_switch();

  //LEDテープ関係
  strip.Begin();
  for(int i=0;i<NUM_LEDS;i++){
    strip.SetPixelColor(i,predefinedColors[i%NUM_OF_COLORS]);
  }
  strip.Show();


  //WiFi,REST関係
  // wifi_connect(wifi_ssid,wifi_password);
  // server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  // server.onNotFound(notFound);
  // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  // server.begin();
  // SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
  // server.on("/push", HTTP_PUT, [] (AsyncWebServerRequest *request) {
  //     const String DIRECTION_PARAM="direction";
  //     const String POWER_PARAM="power";
  //     const String DURATION_PARAM="duration";
  //     char direction='\0';
  //     int power=0;
  //     int duration=1000;

  //     if (request->hasParam(DIRECTION_PARAM)) {
  //         String direction_str = request->getParam(DIRECTION_PARAM)->value();
  //         direction=direction_str.charAt(0);
  //     }else{
  //       request->send(400, "text/plain", "DIRECTION ERROR");
  //       return;
  //     }

  //     if (request->hasParam(POWER_PARAM)) {
  //         String power_str = request->getParam(POWER_PARAM)->value();
  //         if(!isNumber(power_str)){
  //           request->send(400, "text/plain", "POWER_PARAM should be number");
  //           return;
  //         }
  //         power = power_str.toInt(); // int型に変換
  //     }else{
  //       request->send(400, "text/plain", "POWER_PARAM mandatory");
  //       return;
  //     }

  //     if (request->hasParam(DURATION_PARAM)) {
  //         String duration_str = request->getParam(DURATION_PARAM)->value();
  //         if(!isNumber(duration_str)){
  //           request->send(400, "text/plain", "DURATION_PARAM should be number");
  //           return;
  //         }
  //         duration = duration_str.toInt(); // int型に変換
  //     }

  //   push(direction, power,duration);
  //   request->send(200, "text/plain", "PUSHED");
  //   printLine(String(direction)+":P"+String(power)+",D"+String(duration));

  // });

  printLine("Process Start!");
  setAllLEDs(predefinedColors[Black]);
}

void loop() {
  static uint32_t next_servo_interuption=0;
  static uint32_t next_heartbeat_time=0;

  M5.update();

  if (SerialBT.available()) {
      char incomingByte = SerialBT.read();
      // 改行文字を受信したらコマンド終了
      if (incomingByte == '\n' || incomingByte == '\r') {
        processReadData();
      } else {
        incomingData += incomingByte; // 文字を追加
      }
  }

  auto current_time = millis();
  if(emag_enable_until < current_time){
    if(emugState!=EmugState::NEUTORAL){
      change_emag_state(EmugState::NEUTORAL);
    }
  }

  if (current_time > next_servo_interuption){
    move_servo(current_time);
    next_servo_interuption=current_time+servo_interuption_interval;
  }

  if (current_time > next_heartbeat_time){
    checkBoxPosition();
    showCatPosition(catState,catPosition);
    next_heartbeat_time = current_time + HEARTBEAT_INTERVAL;
  }

  if(catState == CatState::APPEARING){
    bool isAppeared = showAppearanceTransition(current_time);
    if(isAppeared){
      catState = CatState::EXIST;
    }
  }

  if(M5.BtnA.wasClicked()){
    ESP.restart();
  }


}