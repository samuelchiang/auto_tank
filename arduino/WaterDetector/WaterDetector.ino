#include <ESP8266WiFi.h> 
#include <PubSubClient.h>

const char* ssid = "Your SSID";
const char* password = "Your Wifi Password";
const char* mqttServer = "Your MQTT Server IP";
const char* mqttUserName = "Your MQTT Server Username";
const char* mqttPwd = "Your MQTT Server Password";

const char* clientID = "wemos";      // 用戶端ID，隨意設定。
const char* DeviceId = "WaterDetector";
const char* EventTopic = "event/WaterDetector";

const int WaterDetPin = A0;
const int LEDPin = BUILTIN_LED;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

const double sensVal = 300.0;
int duration = 200; // 200 miliseconds
#define ArrayLenth  10
int volArray[ArrayLenth];   //Store the average value of the sensor feedback
int volArrayIndex=0;


//--------------- Wifi
void setup_wifi() {
  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//--------------- MQTT
void mqttReconnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientID, mqttUserName, mqttPwd)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // 等5秒之後再重試
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

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}


void checkStateChange() {
  volArray[volArrayIndex++]=analogRead(WaterDetPin);
  if(volArrayIndex==ArrayLenth)volArrayIndex=0;
  double voltageWater = avergearray(volArray, ArrayLenth); 
  Serial.println(voltageWater);
  
  if (voltageWater > sensVal) {
    digitalWrite(LEDPin, HIGH);
    //Serial.println("Send Data to server!!!");  
    String msgStr = "";      // 暫存MQTT訊息字串
    msgStr=msgStr+"{ \"id\":\""+DeviceId+"\", \"voltage\":"+voltageWater+"}";
    mqtt_publish(EventTopic, msgStr);
    delay(duration);
  } else {
    digitalWrite(LEDPin, LOW);
    delay(duration);
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(LEDPin, OUTPUT);
  pinMode(WaterDetPin, INPUT_PULLUP);
  digitalWrite(LEDPin, LOW);

  setup_wifi();
  mqttClient.setServer(mqttServer, 1883); 
}

void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  checkStateChange();
  
}
