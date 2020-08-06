#include <PZEM004Tv30.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <SPIFFS.h>
#include <WiFiUdp.h>

const int LEDPin = BUILTIN_LED;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqttServer[40] = "";  // MQTT伺服器位址
char mqttPort[6]  = "1883";
char mqttUserName[32] = "";  // 使用者名稱
char mqttPwd[32] = "";  // MQTT密碼
char DeviceId[32] = "relaybox_n";  // Device ID

char EventTopic[64];
char CommandTopic[64];
char PowerTopic[64];

//flag for saving data
bool shouldSaveConfig = false;

PZEM004Tv30 pzem(&Serial2);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

int RELAYpins[8] = {32, 33, 25, 26, 27, 14, 4, 13};
//GPIO 32, 33, 25, 26, 27, 14, 4, 13
//for switch 1,2,3,4,5,6,7,8


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//--------------- Wifi
void setup_wifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  //WifiManager 還提供一個特別的功能，就是可以在 Config Portal 多設定其他的參數，
  // 這等於是提供了參數的設定介面，可以避免將參數寫死在程式碼內。流程如下所述
  //     1. 開機時，讀取 SPIFFS, 取得 config.json 檔案，讀入各參數，若沒有，則採用預設值
  //     2. 若 ESP 32 進入 AP 模式，則在 Config Portal 可設定參數
  //     3. 設定完之後，將參數寫入 SPIFFS 下的 config.json
  //
  // setup custom parameters
  // 
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqttServer("mqttServer", "mqtt server", mqttServer, 40);
  WiFiManagerParameter custom_mqttPort("mqttPort", "mqtt port", mqttPort, 6);
  WiFiManagerParameter custom_mqttUserName("mqttUserName", "mqtt user name", mqttUserName, 32);
  WiFiManagerParameter custom_mqttPwd("mqttPwd", "mqtt password", mqttPwd, 32);
  WiFiManagerParameter custom_DeviceId("DeviceId", "Device ID", DeviceId, 32);
   
  //add all your parameters here
  wm.addParameter(&custom_mqttServer);
  wm.addParameter(&custom_mqttPort);
  wm.addParameter(&custom_mqttUserName);
  wm.addParameter(&custom_mqttPwd);
  wm.addParameter(&custom_DeviceId);
  
  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  //res = wm.autoConnect("SmartPlug"); // anonymous ap
  //若重啟後，還是連不上家裡的  Wifi AP, ESP32 就會進入設定的 AP 模式，必須將這個模式設定三分鐘 timeout 後重啟，
  //萬一家裡的 Wifi AP 恢復正常，ESP32 就可以直接連線。這裡的三分鐘 timeout, 經過測試，只要有在 Config Portal 頁面操作，
  //每次都會重置三分鐘timeout，所以在設定時，不需緊張。
  wm.setConfigPortalTimeout(180);//seconds
  res = wm.autoConnect("relaybox","1qaz2wsx"); // password protected ap
  if(!res) {
      Serial.println("Failed to connect");
      ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }

  //read updated parameters
  strcpy(mqttServer, custom_mqttServer.getValue());
  strcpy(mqttPort, custom_mqttPort.getValue());
  strcpy(mqttUserName, custom_mqttUserName.getValue());
  strcpy(mqttPwd, custom_mqttPwd.getValue());
  strcpy(DeviceId, custom_DeviceId.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument doc(1024);

    doc["mqttServer"]   = mqttServer;
    doc["mqttPort"]     = mqttPort;
    doc["mqttUserName"] = mqttUserName;
    doc["mqttPwd"]      = mqttPwd;
    doc["DeviceId"]     = DeviceId;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJsonPretty(doc, Serial);
    serializeJson(doc, configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }  
}


//--------------- MQTT
void mqttReconnect() {
  int countdown=5;
  while (!mqttClient.connected()) {
    if (mqttClient.connect(DeviceId, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
      mqttClient.subscribe(CommandTopic);
      mqttClient.setCallback(mqttCallback);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // 等5秒之後再重試'
      //設置 timeout, 過了 25 秒仍無法連線, 就重啟 EPS32
      countdown--;
      if(countdown==0){
        Serial.println("Failed to reconnect");
        ESP.restart();
      }
    }
  }
}

void mqtt_publish(const char* topic, String str){
    // 宣告字元陣列
    byte arrSize = str.length() + 1;
    char msg[arrSize];
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" message: ");
    Serial.print(str);
    Serial.print(" arrSize: ");
    Serial.println(arrSize);
    str.toCharArray(msg, arrSize); // 把String字串轉換成字元陣列格式
    if (!mqttClient.publish(topic, msg)){
      Serial.println("Faliure to publish, maybe you should check the message size: MQTT_MAX_PACKET_SIZE 128");       // 發布MQTT主題與訊息
    }
}

void setup_relay(){
  for(int n=0;n<8;n++){
    pinMode(RELAYpins[n],OUTPUT);
    digitalWrite(RELAYpins[n], HIGH);
  }
}

void report_relay(){
    static unsigned long samplingTime = millis();
    unsigned long samplingInterval = 10000; //ms
    
    if(millis()-samplingTime > samplingInterval)
    {
      for(int n=0;n<8;n++){
        char topic[30];
        sprintf(topic, "%sswitch%d",EventTopic, n+1);
        //Serial.println(topic); 
        int status = digitalRead (RELAYpins[n]);
        if (n+1 <=3 ){
          if (status==0){
            status = 1;
          }else{
            status = 0;
          }
        }
        mqtt_publish(topic, String(status));
      }

      samplingTime=millis();
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
   if(strcmp(topic, "cmd/relaybox/pzem") == 0 && memcmp(payload, "reset", 5) == 0){
      Serial.println("pzem resetEnergy");  
      pzem.resetEnergy();
      return;
   }
   
  //if cmd == 0 => turn off
  //if cmd == 1 => turn on
  int cmd = (int)*payload - '0';
  Serial.print(cmd);
  int sn = (int)topic[19] - '0';
  //Serial.print(" sn ");
  //Serial.print(sn);  
  if ((sn <1 ) || (sn > 8 )){
    Serial.println(" wrong switch number");  
    return;
  }
  int pin = RELAYpins[sn-1];
  Serial.print(" pin ");
  Serial.print(pin);  
  Serial.println();

  //if cmd is encryped, decode it first
  // cmd_plain_text = AES_decode(cmd)
  // check if timestamp from cmd_plain_text is send within 5 seconds, otherwise drop it.

  if (sn <=3 ){
    if (cmd==0){
      digitalWrite(pin, HIGH);  
    }else{
      digitalWrite(pin, LOW);
    }
  }else{
    if (cmd==0){
      digitalWrite(pin, LOW);  
    }else{
      digitalWrite(pin, HIGH);
    }
  } 
}


void report_power(){
    static unsigned long samplingTime = millis();
    unsigned long samplingInterval = 10000; //ms
    static int countdown=5;
    
    if(millis()-samplingTime > samplingInterval)
    {
      Serial.println();

      float voltage = pzem.voltage();
      float current = pzem.current();
      float power = pzem.power();
      float energy = pzem.energy();
      float frequency = pzem.frequency();
      float pf = pzem.pf();

      // 組合MQTT訊息；
      String msgStr = "";      // 暫存MQTT訊息字串
      msgStr=msgStr+"{ \"v\":"+voltage+",  \"i\":"+current+", \"p\":"+power+", \"e\":"+energy+" , \"f\":"+frequency+", \"pf\":"+pf+" }";
      
      if( isnan(voltage) || 
          isnan(current) || 
          isnan(power) || 
          isnan(energy) || 
          isnan(frequency) || 
          isnan(pf)){
          
          Serial.print("isnan: ");
          Serial.println(msgStr);      
          countdown--;
          if(countdown==0){
            Serial.println("Failed to connect PZEM");
            ESP.restart();
          }
      } else {
        countdown=5;
        mqtt_publish(PowerTopic, msgStr);
      }
      msgStr = "";
      samplingTime=millis();
    }
}

void setup_topic() {
  sprintf(&EventTopic[0],"status/%s/", DeviceId);  //"status/relaybox_n/";
  sprintf(&CommandTopic[0],"cmd/%s/#", DeviceId);  //"cmd/relaybox_n/#";
  sprintf(&PowerTopic[0],"power/%s/", DeviceId);  //"power/relaybox_n/";
}

void setup_spiffs(){
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);
     
        deserializeJson(doc, buf.get(), DeserializationOption::NestingLimit(20));
        serializeJsonPretty(doc, Serial);

        if (!doc.isNull()) {
          Serial.println("\nparsed json");

          if (doc.containsKey("mqttServer")){
            strcpy(mqttServer, doc["mqttServer"]);  
          }
          if (doc.containsKey("mqttPort")){
            strcpy(mqttPort, doc["mqttPort"]);
          }
          if (doc.containsKey("mqttUserName")){
            strcpy(mqttUserName, doc["mqttUserName"]);
          }
          if (doc.containsKey("mqttPwd")){
            strcpy(mqttPwd, doc["mqttPwd"]);
          }
          if (doc.containsKey("DeviceId")){
            strcpy(DeviceId, doc["DeviceId"]);
          }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
  Serial.begin(115200);
  setup_spiffs(); // Read parameter from FS, if no data, use default
  setup_wifi(); // If running on AP mode, get the paramters from config portal
  setup_topic(); // Configure Topic with Device ID
  mqttClient.setServer(mqttServer, atoi(mqttPort)); 
  setup_relay();
}

void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  report_relay();
  //delay(60000); //Cannot use delay here, or the mqtt connnection will lose
  
  report_power();
}
