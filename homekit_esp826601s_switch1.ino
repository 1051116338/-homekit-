
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

//支持阳阳学编程的web wifi配网，暂时不开源
//#define YYXBC_WEBCONFIG

//支持homekit
#define YYXBC_HOMEKIT

//支持433自锁开关功能，需要用蜂鸟无线公司出的远-R1 433模块，
//将R1的VCC和GND分别接在esp-01s的VCC和GND上，两个DAT引脚的随意一个接在esp-01s的RX上
#define YYXBC_WITH_433

//依赖库 LinkedList
//https://github.com/ivanseidel/LinkedList
#include <LinkedList.h>

#if (defined(YYXBC_WEBCONFIG))
  #include "wificfg.h"
#endif


#if (defined(YYXBC_HOMEKIT))
//https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266
  #include <arduino_homekit_server.h>
#endif

#if (defined(YYXBC_WITH_433))
  #include <LittleFS.h>
  #include <FS.h>
  #include <RCSwitch.h>
  RCSwitch mySwitch = RCSwitch();
  //433指令数据格式
  struct RawData{
    uint16_t bits;
    uint16_t protocol;
    long unsigned data;
    RawData():protocol(0),data(0){}
    
  };
//433指令列表
LinkedList<RawData*> mySwitchList;
#define MYFS LittleFS
#endif

String version  = "1.0.6";

//Esp-01/01s，继电器接GPIO0,物理开关接GPIO2
#define LED_BUILTIN_LIGHT 0
#define LED_BUILTIN_K2 2
#define LED_BUILTIN_K3 3
#define LED_BUILTIN_KED 2

////NodeMCU 继电器接D3,物理开关接D4
//#define LED_BUILTIN_LIGHT D3
//#define LED_BUILTIN_K2 D4

/***
 * 继电器高电平触发时，YYXBC_HIGH = 1，YYXBC_LOW  = 0
 * 继电器低电平触发时，YYXBC_HIGH = 0，YYXBC_LOW  = 1
 */
const int YYXBC_HIGH = 0 ;
const int YYXBC_LOW  = 1 ;

/***
 * 物理开关点动模式1，自锁模式0
 */
const int YYXBC_BUTTON_TYPE = 0;

const char *ssid = "TP8888";
const char *password = "aazz6993..";


#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void wifi_connect(String ssid,String  password) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.println("WiFi connecting...");
  while (!WiFi.isConnected()) {
    delay(100);
    Serial.print(".");
  }
  Serial.print("\n");
  Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
}

//是否处理联网状态
bool isNetConnected(){return (WiFi.status() == WL_CONNECTED);}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 按下BlinkerAPP按键即会执行该函数
void switch_callback( bool state)
{
    LOG_D("get switch state: ", state);
    
    if (state == true) {
        LOG_D("Toggle on!");
        digitalWrite(LED_BUILTIN_LIGHT, YYXBC_HIGH);
    }
    else if (state == false) {
        LOG_D("Toggle off!");
        digitalWrite(LED_BUILTIN_LIGHT, YYXBC_LOW);
    }
}

/*
*连续10次homekit开关，时间1秒内，认为是要进入配网模式，重启进入配网模式
*/
void configcheck(){
  //APP开关开、关次数，1秒内计入
  static int physics_count  = 0;
  static int lastphysicsms = 0;
  Serial.println("homekit开关被触发");
  Serial.println(millis() - lastphysicsms);
  if (millis() - lastphysicsms < 1000) {
    Serial.printf("(%u/5),homekit开关触发配网模式...",physics_count);
    physics_count++;
  }else{
    physics_count = 0;  
  }
  lastphysicsms = millis();
  //进入配网模式，重启esp8266
  if(physics_count > 10){
    Serial.println("正在重启esp8266，启动后进入配网模式...");
     #if (defined(YYXBC_WEBCONFIG))
      WIFI_RestartToCfg();
     #else
     ESP.restart();
    #endif
  }
}
/////////////////////////////homekit ////////////////////////////////
// access your HomeKit characteristics defined in my_accessory.c
//extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_switch_on;
extern "C" void init_accessory_identify(const char* serialNumber,const char* password) ;


static uint32_t next_heap_millis = 0;

//Called when the switch value is changed by iOS Home APP
void cha_switch_on_setter(const homekit_value_t value) {
  bool on = value.bool_value;
  cha_switch_on.value.bool_value = on;  //sync the value
  LOG_D("Switch: %s", on ? "ON" : "OFF");
  if (on) {
      LOG_D("Toggle on!");
      digitalWrite(LED_BUILTIN_LIGHT, YYXBC_HIGH);
  }
  else {
      LOG_D("Toggle off!");
      digitalWrite(LED_BUILTIN_LIGHT, YYXBC_LOW);
  }
  
  //连续10次homekit开关，时间1秒内，认为是要进入配网模式，重启进入配网模式
  configcheck();

}

void my_homekit_setup() {
  cha_switch_on.setter = cha_switch_on_setter;
  Settings& cfg = getSettings();
//    config.password = (char*)cfg.auth.c_str();cha_switch_on
//  arduino_homekit_setup(&config);
    init_accessory_identify((char*)cfg.sn.c_str(),(char*)cfg.auth.c_str());  
 
}

void my_homekit_loop() {
  arduino_homekit_loop();
}

/////////////////////////////////////main/////////////////////////////////

// 定义一个标志变量，用于指示按钮是否被按下
volatile bool button_pressed = false;

// 中断处理函数
void IRAM_ATTR button_handler() {
  button_pressed = true; // 将按钮按下标志设为true
}


/*
*程序入口函数
**/
void setup() {

//   
    // 初始化串口
    Serial.begin(115200);

    // 初始化有LED的IO
    pinMode(LED_BUILTIN_LIGHT, OUTPUT);
    digitalWrite(LED_BUILTIN_LIGHT, YYXBC_LOW);
    
    pinMode(LED_BUILTIN_K2, OUTPUT);
//    digitalWrite(LED_BUILTIN_K2, LOW);

  // 将按钮的中断处理函数button_handler与下降沿触发模式关联
  attachInterrupt(digitalPinToInterrupt(LED_BUILTIN_K2), button_handler, FALLING);




  #if(defined(YYXBC_WITH_433))
      //load 433 configure
    loadConfig(&MYFS);
    pinMode(LED_BUILTIN_K3, FUNCTION_3); 
    mySwitch.enableReceive(LED_BUILTIN_K3);
    Serial.println("started RCSwitch");
  #endif  
  
 #if (defined(YYXBC_WEBCONFIG))
    //wifi 配网
    attach(loop_callback);
    bool ret = WIFI_Init();
    Settings& cfg = getSettings();
    Serial.println("ret=");
    Serial.println(ret);
    //if(ret){
//      Serial.println("started homekit_storage_reset()");
//      homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
    //}
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin();

 #else
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WiFi.SSID().c_str(),WiFi.psk().c_str());
 #endif  
 
  Serial.println("WiFi connecting...");
  int n = 0;
  while (!WiFi.isConnected()) {
      delay(100);
      if(n++%20 == 0){
         Serial.println("#");
         Serial.println(n);
         Serial.print("#");         
      }
  }

   #if (defined(YYXBC_HOMEKIT))
//    homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
    my_homekit_setup();
  #endif

  //report the switch value to HomeKit if it is changed 
  bool btnRet  = digitalRead(LED_BUILTIN_LIGHT) == YYXBC_HIGH ? true :false;
  updateHomeKitUi( btnRet );

  Serial.print("\n");
  Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
 
}

void loop() {

    static int lastms = millis();
    if (millis()-lastms > 30000) {
      lastms = millis();
      Serial.printf(PSTR("Running version %s for %d Free mem=%d\n"),
          WiFi.localIP().toString().c_str(),
          version.c_str(), lastms/1000, ESP.getFreeHeap());
    }
    

    //检查物理开关状态
    bool bret = false;
    if(YYXBC_BUTTON_TYPE == 1){
       bret = btnHandler1();
    }else{
       bret = btnHandler2();
    }

  #if (defined(YYXBC_HOMEKIT))
        //homekit loop
      my_homekit_loop();
  #endif

   /*
    *连续5次开关，时间5秒内，认为是要进入配网模式，重启进入配网模式
    */
    //物理开关开、关次数，1秒内计入
    static int physics_count  = 0;
    static int lastphysicsms = 0;
    if(bret ){
      Serial.println("物理开关被触发");
      Serial.println(millis()-lastphysicsms);
      if (millis()-lastphysicsms < 1000) {
        Serial.printf("%u/5物理开关触发配网模式...",physics_count);
        physics_count++;
      }else{
        physics_count = 0;  
      }
      lastphysicsms = millis();
    }
    //进入配网模式，重启esp8266
    if(physics_count > 5){
        Serial.println("正在重启esp8266，启动后进入配网模式...");
       #if (defined(YYXBC_WEBCONFIG))
        WIFI_RestartToCfg();
       #else
       ESP.restart();
      #endif
    } 

   #if(defined(YYXBC_WITH_433))
   if (mySwitch.available()) {
      uint16_t bit = mySwitch.getReceivedBitlength();
      uint16_t protocol = mySwitch.getReceivedProtocol();
      long unsigned switchKey = mySwitch.getReceivedValue();
      Serial.printf("Received :%u,/bit %u,Protocol %u \r\n",switchKey,bit,protocol);
      if(bit > 0 ){
        if( findRawData( bit, protocol,switchKey )){
          mySwitchHandler();
        }else{
          Serial.println("不执行");
        }
      } 
      mySwitch.resetAvailable();
    }
  #endif  
    
}

//点动模式按钮，监听按钮状态，执行相应处理
bool btnHandler1()
{
  static bool oButtonState = false;
  int state1 =  digitalRead(LED_BUILTIN_K2); //按钮状态
  int state2 =  digitalRead(LED_BUILTIN_LIGHT); //灯的状态
  if(state1 == LOW)
  {
    if(oButtonState){
      if(state2 == YYXBC_HIGH )
      { 
        switch_callback(false);
        updateHomeKitUi( false );
        Serial.println("按钮对灯已执行关闭");
      }else{
        switch_callback(true);
        updateHomeKitUi( true );
        Serial.println("按钮对灯已执行打开");
      }
      oButtonState = false;
      return true;
    }
    
  }else{
      oButtonState = true;
  }
  

  return false;
}

//自锁模式按钮，监听按钮状态，执行相应处理
bool btnHandler2()
{
  static bool is_btn = digitalRead(LED_BUILTIN_K2);//按钮的标志位，用来逻辑处理对比，判断按钮有没有改变状态
  bool is = digitalRead(LED_BUILTIN_K2);   //按钮状态
  if ( is != is_btn)
  {
    bool is_led = digitalRead(LED_BUILTIN_LIGHT);
    digitalWrite(LED_BUILTIN_LIGHT, !is_led);
    if (is_led == YYXBC_HIGH)
    {
      switch_callback(false);
      updateHomeKitUi( false );
      Serial.println("按钮对灯已执行关闭");
    }
    else
    {
      switch_callback(true);
      updateHomeKitUi( true );
      Serial.println("按钮对灯已执行打开");
    }
    is_btn = digitalRead(LED_BUILTIN_K2);  //更新按钮状态
    return true;
  }
 return false;
}

void updateHomeKitUi(bool state){
     //report the switch value to HomeKit if it is changed 
    bool switch_is_on =  state ;
    cha_switch_on.value.bool_value = switch_is_on;
    homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
    LOG_D("updateHomeKitUi  state %u !",switch_is_on);
}

//自锁模式按钮，监听433按钮状态，执行相应处理
bool btnHandler3()
{
  static bool is_btn = digitalRead(LED_BUILTIN_K3);//按钮的标志位，用来逻辑处理对比，判断按钮有没有改变状态
  bool is = digitalRead(LED_BUILTIN_K3);   //按钮状态
  if ( is != is_btn)
  {
    bool is_led = digitalRead(LED_BUILTIN_LIGHT);
    digitalWrite(LED_BUILTIN_LIGHT, !is_led);
    if (is_led == YYXBC_HIGH)
    {
      switch_callback(false);
      updateHomeKitUi( false );
      Serial.println("按钮对灯已执行关闭");
    }
    else
    {
      switch_callback(true);
      updateHomeKitUi( true );
      Serial.println("按钮对灯已执行打开");
    }
    is_btn = digitalRead(LED_BUILTIN_K3);  //更新按钮状态
    return true;
  }
 return false;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool findRawData(uint16_t bit,uint16_t protocol,long unsigned data )
{
 int nSize = mySwitchList.size();
  for(int i = 0;i< nSize ;i++){
    RawData* pRawData = mySwitchList.get(i);
    if(pRawData){
      if(bit == pRawData->bits &&
      protocol == pRawData->protocol &&
      data == pRawData->data){
        return true;
      }
    }
  }
  return false;
}

void addRawData(uint16_t bit,uint16_t protocol,long unsigned data ){
  if(findRawData(bit,protocol,data)){
    Serial.println("433指令已经存在，不作处理");
    return;
  }
  RawData* tempRawData_   =  new RawData();
  tempRawData_->bits = bit;
  tempRawData_->protocol = protocol;
  tempRawData_->data = data;
  mySwitchList.add(tempRawData_);
  Serial.println("指令对码完成");
  
}

bool saveConfig(FS *fs){
  if (!fs->begin()) {
    Serial.println("Unable to begin(), aborting");
    return false;
  }
  Serial.println("Save file, may take a while...");
  long start = millis();
  File f = fs->open("/433config.bin", "w");
  if (!f) {
    Serial.println("Unable to open 433config for writing, aborting");
    return false;
  }
  int nSize = mySwitchList.size();
  int nwrite = f.write((char*)&nSize, sizeof(uint16_t));
  // assert(nwrite != sizeof(uint16_t));
  for(int i = 0;i< nSize ;i++){
    RawData* pRawData = mySwitchList.get(i);
    if(pRawData){
      f.write((char*)&pRawData->bits, sizeof(uint16_t));
      f.write((char*)&pRawData->protocol, sizeof(uint16_t));
      f.write((char*)&pRawData->data, sizeof(long unsigned));
    }
  }
  f.close();
  long stop = millis();
  Serial.printf("==> Time to write  chunks = %ld milliseconds\n", stop - start);
  return true;
}

bool loadConfig(FS *fs){
if (!fs->begin()) {
    Serial.println("Unable to begin(), aborting");
    return false;
  }
  Serial.println("Reading file, may take a while...");
  long start = millis();
  File f = fs->open("/433config.bin", "r");
  if (!f) {
    Serial.println("Unable to open file for reading, aborting");
    return false;
  }
  int nSize = 0;
  int nread = f.read((uint8_t*)&nSize, sizeof(uint16_t));
  // assert(nread != sizeof(uint16_t));
   Serial.println(nread);
  for(int i = 0;i< nSize ;i++){
    RawData* pRawData = new RawData();
    if(pRawData){
      f.read((uint8_t*)&pRawData->bits, sizeof(uint16_t));
      f.read((uint8_t*)&pRawData->protocol, sizeof(uint16_t));
      f.read((uint8_t*)&pRawData->data, sizeof(long unsigned));
      Serial.printf("bit:%u,protocol:%u,data:%u \r\n",pRawData->bits,pRawData->protocol,pRawData->data);
      mySwitchList.add(pRawData);
    }
  }
  f.close();
  long stop = millis();
  Serial.printf("==> Time to write  chunks = %ld milliseconds\n", stop - start);
  return true;
}


/**
*配网模式下，loop在这里执行
*/
int loop_callback (){
  
//  static int lastms = millis();
//  if (millis()-lastms > 3000) {
//    lastms = millis();
//    Serial.printf(PSTR("配网模式 version %s for %d Free mem=%d\n"),version.c_str(), lastms/1000, ESP.getFreeHeap());
//  }

  #if(defined(YYXBC_WITH_433))
   if (mySwitch.available()) {

      uint16_t bit = mySwitch.getReceivedBitlength();
      uint16_t protocol = mySwitch.getReceivedProtocol();
      long unsigned switchKey = mySwitch.getReceivedValue();
      Serial.printf("Received :%u,bit %u,Protocol %u \r\n",switchKey,bit,protocol);
  
      if(bit > 0 ){
        if(findRawData( bit, protocol,switchKey )){
          mySwitchHandler();
        }else{
          addRawData( bit, protocol,switchKey);
          saveConfig(&MYFS);
        }
      }

      //LED灯常亮5秒
      digitalWrite(LED_BUILTIN_KED, HIGH);
      delay(5000);
      digitalWrite(LED_BUILTIN_KED, LOW);

      mySwitch.resetAvailable();
    }
  #endif  
  
  return 0;
}

//自锁模式按钮，监听433按钮状态，执行相应处理
bool mySwitchHandler()
{
  static long lastphysicsms = 0;
  if (millis()-lastphysicsms > 500) {
      bool is_led = digitalRead(LED_BUILTIN_LIGHT);
      digitalWrite(LED_BUILTIN_LIGHT, !is_led);
      if (is_led == YYXBC_HIGH)
      {
        switch_callback(false);
        updateHomeKitUi( false );
        Serial.println("按钮对灯已执行关闭");
      }
      else
      {
        switch_callback(true);
        updateHomeKitUi( true );
        Serial.println("按钮对灯已执行打开");
      }
      is_led = digitalRead(LED_BUILTIN_LIGHT);  //更新按钮状态
      lastphysicsms = millis();
      
      return true;
  }

 return false;
}
