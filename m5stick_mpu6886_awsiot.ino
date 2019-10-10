#include <M5StickC.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

float accX = 0;
float accY = 0;
float accZ = 0;

float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

float temp = 0;

// WiFi Setting
const char* ssid = "<YOUR_SSID>";
const char* password = "<YOUR_WIFI_PASSWORD>";

// AWS IoT Setting
const char* awsEndpoint = "<AWS_IOT_ENDPOINT>";
// Example: xxxxxxxxxxxxxx.iot.ap-northeast-1.amazonaws.com

const int awsPort = 8883;
const char* pubTopic = "m5stickc_sensor";
const char* rootCA = "-----BEGIN CERTIFICATE-----\n" \
"......" \
"-----END CERTIFICATE-----\n";
const char* certificate = "-----BEGIN CERTIFICATE-----\n" \
"......" \"-----END CERTIFICATE-----\n";
const char* privateKey = "-----BEGIN RSA PRIVATE KEY-----\n" \
"......" \"-----END RSA PRIVATE KEY-----\n";

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);

long messageSentAt = 0;
int dummyValue = 0;
char pubMessage[256];

void setup_wifi(){
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // For WiFi Error
  WiFi.disconnect(true);
  delay(5000);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_awsiot(){
  httpsClient.setCACert(rootCA);
  httpsClient.setCertificate(certificate);
  httpsClient.setPrivateKey(privateKey);
  mqttClient.setServer(awsEndpoint, awsPort);
  mqttClient.setCallback(mqttCallback);
}

void connect_awsiot() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "<YOUR_DEVICE_NAME>";
    //clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying＿
      delay(5000);
    }
  }
}

void mqttCallback (char* topic, byte* payload, unsigned int length) {
    Serial.print("Received. topic=");
    Serial.println(topic);
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.print("\n");
}

void setup(){
  // Initialize the M5Stack object
  M5.begin();

  delay(10000) ;
  setup_wifi();
  setup_awsiot();
  
  configTime(9*3600L,0,"ntp.nict.jp","time.google.com","ntp.jst.mfeed.ad.jp");
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  Serial.printf("%02d-%02d-%02dT%02d:%02d:%02d+09:00\n", timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 15);
  M5.Lcd.println("  X       Y       Z");
  M5.MPU6886.Init();
}

int milliSec=0;
int previousSec=0;
int period=5;
void loop(){
 if (!mqttClient.connected()) {
    connect_awsiot();
  }
    
    mqttClient.loop();

  long now = millis();
  if ((messageSentAt-now>0)||(now - messageSentAt > 500)) {
    M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);
    M5.MPU6886.getAccelData(&accX,&accY,&accZ);
    M5.MPU6886.getTempData(&temp);
  
    M5.Lcd.setCursor(0, 30);
    M5.Lcd.printf("%.2f   %.2f   %.2f      ", gyroX, gyroY,gyroZ);
    M5.Lcd.setCursor(140, 30);
    M5.Lcd.print("o/s");
    M5.Lcd.setCursor(0, 45);
    M5.Lcd.printf("%.2f   %.2f   %.2f      ",accX * 1000,accY * 1000, accZ * 1000);
    M5.Lcd.setCursor(140, 45);
    M5.Lcd.print("mg");
    M5.Lcd.setCursor(0, 60);
    M5.Lcd.printf("Temperature : %.2f C", temp);

    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char timeMessage[128];
    sprintf(timeMessage,"%02d-%02d-%02dT%02d:%02d:%02d.%03d+09:00", timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday,timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,milliSec);
    
    messageSentAt = now;
    sprintf(pubMessage, "{\"timestamp\":\"%s\",\"gyroX\": %.2f,\"gyroY\": %.2f,\"gyroZ\": %.2f,\"accX\": %.2f,\"accY\": %.2f,\"accZ\": %.2f,\"temp\": %.2f}",
    timeMessage,gyroX,gyroY,gyroZ,accX * 1000,accY * 1000, accZ * 1000,temp);
    Serial.print("Publishing message to topic ");
    Serial.println(pubTopic);
    Serial.println(pubMessage);
    mqttClient.publish(pubTopic, pubMessage);
    Serial.println("Published.");
  }
  delay(period);
  
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  if(previousSec!=timeinfo.tm_sec){
    milliSec=0;
    previousSec=timeinfo.tm_sec;
  }else{
    milliSec+=period;
  }
  
}
