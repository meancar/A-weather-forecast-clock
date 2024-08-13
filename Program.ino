#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <FirebaseESP32.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include "icons.h"
#include "time.h"
#include <driver/ledc.h>

// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

#define BUTTON 14
int count = 0;
int button_state;
bool flag;

// Lưu ý, set Delay lớn hơn 150ms nếu không MAX30100 sẽ bị timeout và ko nhận được giá trị

const char* ssid = "Redmi Note 10 Pro";
const char* password = "09072021";

Adafruit_SSD1306 display(128, 64, &Wire, -1);


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 7*3600; //tương ứng với UTC+7
const int daylightOffset_sec = 0;

String lat = "10.850";
String lon = "106.771";
String openWeatherMapApiKey = "";

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

#define FIREBASE_HOST ""
#define API_KEY ""
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
#define USER_EMAIL ""
#define USER_PASSWORD ""
//doan1khalin@gmail.com
//khalin@123@

String jsonBuffer;
double speed, humidity, pressure, temperature;
int weatherid;
int hrs,mins,secs,date,mon,year,day;


const String weekDays[7]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const String months[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
char daymonyear[20];
char hms[20];

const int NUM_POINTS = 60;
const int RADIUS = 28;

int pointsX[NUM_POINTS];
int pointsY[NUM_POINTS];

PulseOximeter pox;
uint8_t tsLastReport = 0;
#define REPORTING_PERIOD_MS 5000
bool maxflag;
float heartbeatSS[10],heartbeat;
int spo2SS[10],spo2;

#define DHT_PIN 33
#define MQ2pin 34
DHT dht(DHT_PIN, DHT11);
float humiSS[5],gasValSS[5],tempSS[5];
float humiditySS,temperatureSS;
int gasSS;

#define buzzPin 32
const int freq = 1000; // f = 1kHz
const int resolution = 8; // 8 bit (0-16bit)
const int channel = 0; //0 - 15

void setup() {
  Serial.begin(9600);
  //OLED config
  setupOLED();
  // WIFI config
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
    }
  //  Serial.println();
  // Serial.println(""); // Hiện Serial gây trễ thời gian cảm biến gây ra quá trình Timeout
   
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  //BUTTON and BUzzeR config
  pinMode(BUTTON,INPUT);
  pinMode(buzzPin, OUTPUT); // Khai báo chân kết nối của buzzer là OUTPUT
  ledcSetup(channel, freq, resolution);//thiết lập ledc với kênh 0, tần số là 1000Hz, resolution là 8
  ledcAttachPin(buzzPin,channel);
  delay(500);
  // Tính toán toạ độ các điểm trên mặt đồng hồ và lưu chung vô mảng
  for (int i = 0; i < NUM_POINTS; i++) {
  pointsX[i] = 64 + RADIUS * cos(i * 6.28 / NUM_POINTS);
  pointsY[i] = 32 + RADIUS * sin(i * 6.28 / NUM_POINTS);
  }
  //Set Local Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1,ntpServer2);
  setLocalTime();

  // Get current weather in Thu Duc
  getweatherWidget();

  
  // DHT11 MQ2 
  dht.begin();
  // getDHTandMq2();

  
  //MAX30100 call back
  pox.setOnBeatDetectedCallback(onBeatDetected);

  //Firebase config
  config.api_key = API_KEY;
  config.host = FIREBASE_HOST;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true); 
  Serial.println("Timer set to 10 seconds (timerDelay variable), it will take 10 seconds before publishing the first reading.");
}

void loop() {
  ledcWrite(0, 255);
  btn_choice();
  menu(count);
}

void btn_choice(){
    button_state = digitalRead(BUTTON);
    if (count == 0) {
      display.drawBitmap(20, 10, logo, 80, 60, 1);
      display.display();
    }
    if (button_state == HIGH) {
      delay(250);
      count++;
      if (count == 3) pox.begin();
      else pox.shutdown();
      // Time out of Max30100 is 150ms
      if (count > 5) count = 1;
    }
    Serial.println(count);
}

void menu(int a){
      switch (a) {
      case 1:
        drawDigitalClock();
        break;
      case 2:
        drawWeatherWidget();
        break;
      case 3:
        drawHeartandO2();
        break;
      case 4:
        drawDHT11andMq2();
        uploadFirebase();
        break;
      case 5:
        drawAnalogClock();
        break;
  }
}
 
void drawText(char *text, int x, int y, bool ln , int textSize ){
  display.setCursor(x,y);
  display.setTextSize(textSize);
  if (ln == true) display.println(text);
  else display.print(text);
}

void setupOLED(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();
}

void getweatherWidget(){
   if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      String serverPath = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&appid=" + openWeatherMapApiKey;
      
      jsonBuffer = httpGETRequest(serverPath.c_str());
      // Serial.println(jsonBuffer);
      JSONVar myObject = JSON.parse(jsonBuffer);
      // chuyển dạng dict thành biến key chứa dữ liệu là value
  
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }
      speed = myObject["wind"]["speed"];
      humidity = myObject["main"]["humidity"];
      pressure = myObject["main"]["pressure"];
      temperature = myObject["main"]["temp"];
      weatherid = myObject["weather"][0]["id"];
      // Serial.print("JSON object = ");
      // Serial.println(myObject);
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  } 
}

void onBeatDetected()
{
    Serial.println("Beat!");
}

void drawWeatherWidget(){
    getweatherWidget();
    display.clearDisplay();
    iconweather(0,20,weatherid);
    display.setCursor(0,10);
    display.print(daymonyear);
    // drawText("ND",56,20,false,1);
    display.drawBitmap(62,0,temp,20,20,1);
    display.setCursor(86,10);
    display.print(String(temperature - 273.15, 2));display.println("C"); // Chuyển đổi nhiệt độ thành chuỗi và hiển thị với 2 chữ số sau dấu thập phân
    // drawText("DA",56,30,false,1);
    display.drawBitmap(62,21,hum,20,20,1);
    display.setCursor(86,30);
    display.print(String(humidity, 2));display.println("%");    // Chuyển đổi độ ẩm thành chuỗi và hiển thị với 2 chữ số sau dấu thập phân
    // drawText("TDG:",56,40,false,1);
    display.drawBitmap(62,42,wind,20,20,1);
    display.setCursor(86,50);
    display.print(String(speed, 2));display.println("m|s");      // Chuyển đổi tốc độ gió thành chuỗi và hiển thị với 2 chữ số sau dấu thập phân
    display.display();
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  http.begin(client, serverName);
  
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode > 0) {
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    // Serial.print("Error code: ");
    // Serial.println(httpResponseCode);
  }
  
  http.end();
  
  return payload;
}
void iconweather(int x, int y, int id){
  switch(id){
    //Mua kem sam set
    case 200:
    case 201:
    case 202:
            display.drawBitmap(x,y,lighting_rain,50,40,1);
            break;
    //Sam set
    case 211:
            display.drawBitmap(x,y,lighting,50,40,1);
            break;
    //May mua
    case 300:
    case 301:
    case 302:
    case 500:
    case 501:
    case 502:
    case 511:
    case 520:
    case 521:
    case 522:
    case 623:
              display.drawBitmap(x,y,rain,50,40,1);
              break;
    //troi khong may
    case 800:
              if(hrs <18 && hrs>5)
                display.drawBitmap(x,y,clear_sky,50,40,1);
               else
                 display.drawBitmap(x,y,night,50,40,WHITE);  
               break;
    //troi co may
    case 700:
    case 711:
    case 721:
    case 731:
    case 741:
    case 751:
    case 801:
    case 802:
    case 803:
              if(hrs <18 && hrs>5)
               display.drawBitmap(x,y,clear_cloudy,50,40,1);
               else
               display.drawBitmap(x,y,night_cloud,50,40,WHITE);
              break;
    // Trời nhiều mây
    case 611:
    case 612:
    case 804:
              display.drawBitmap(x,y,cloudy,50,40,1);
              break;
  }
}

void setLocalTime(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Thoi gian khong co san");
    return;
  }  
  hrs = timeinfo.tm_hour;
  mins = timeinfo.tm_min;
  secs = timeinfo.tm_sec;
  date = timeinfo.tm_wday;
  year = timeinfo.tm_year;
  mon = timeinfo.tm_mon;
  day = timeinfo.tm_mday;

  char formatsecs[2];
  sprintf(formatsecs,"%02d",secs);
  sprintf(daymonyear, "%s|%s|%d", weekDays[date].c_str(), months[mon].c_str(), day);
  sprintf(hms,"%d:%d:%s",hrs,mins,formatsecs);
}

void drawAnalogClock(){
  setLocalTime();
  //(64,32 là trung tâm của đồng hồ)
  //Tính toán các góc của 3 kim
  float secAngle = map(secs, 0, 60, 0, 360);
  float minAngle = map(mins, 0, 60, 0, 360);
  float hrAngle = map(hrs, 1, 12, 30, 360);

  //Tính toán các toạ độ của các kim chuyển về Radian (-90 để chuyển về 0 đọ dứng)
  int hrX = 64 + (RADIUS - 11) * cos((hrAngle - 90) * PI / 180);
  int hrY = 32 + (RADIUS - 11) * sin((hrAngle - 90) * PI / 180);
  int minX = 64 + (RADIUS - 6) * cos((minAngle - 90) * PI / 180);
  int minY = 32 + (RADIUS - 6) * sin((minAngle - 90) * PI / 180);
  int secX = 64 + (RADIUS)*cos((secAngle - 90) * PI / 180);
  int secY = 32 + (RADIUS)*sin((secAngle - 90) * PI / 180);

  display.clearDisplay();  //clear the display buffer

  //draw the clock face
  display.drawCircle(64, 32, 32, WHITE);  //Draw a Center Point
  //Vẽ 12 điểm giờ cuẩ đồng hồ
  for (int i = 0; i < NUM_POINTS; i += 5) {
    display.fillCircle(pointsX[i], pointsY[i], 1, WHITE);
  }

  //Display Numbers 12-3-6-9
  display.setTextSize(1);
  display.setTextColor(WHITE);

  for (int i = 12; i > 1; i -= 3) {
    float angle = map(i, 1, 12, 30, 360);
    int xPos = 64 + (RADIUS - 7) * cos((angle - 90) * PI / 180) - 3;
    int yPos = 32 + (RADIUS - 7) * sin((angle - 90) * PI / 180) - 3;
    display.setCursor(xPos, yPos);
    display.print(i);
  }

  //draw the hour hand
  display.drawLine(64, 32, hrX, hrY, WHITE);
  //draw the minute hand
  display.drawLine(64, 32, minX, minY, WHITE);
  //draw the Second hand
  display.drawLine(64, 32, secX, secY, WHITE);
  // display.drawCircle(secX, secY, 2, WHITE);
  //update the display
  display.display();
}

void updatemax(){
  delay(50);
  for(int i = 0; i < 10; i++){
  pox.update();
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
  heartbeatSS[i] = pox.getHeartRate();
  spo2SS[i] = pox.getSpO2();
  tsLastReport = millis();
   }
  }
}

void drawHeartandO2(){
  updatemax();
  heartbeat = (heartbeatSS[0] + heartbeatSS[1] + heartbeatSS[2] + heartbeatSS[3] +  heartbeatSS[4] + heartbeatSS[5] + heartbeatSS[6] + heartbeatSS[7] + heartbeatSS[8] +  heartbeatSS[9])/10;
  spo2 = (spo2SS[0] + spo2SS[1] + spo2SS[2] + spo2SS[3] + spo2SS[4] + spo2SS[5] + spo2SS[6] + spo2SS[7] + spo2SS[8] + spo2SS[9] )/10;
  display.clearDisplay();
  display.drawBitmap(0,0,heartss,40,30,1);
  display.setCursor(40,10);
  display.setTextSize(2);display.print(heartbeat); display.setTextSize(1);display.print("bm");
  display.drawBitmap(0,32,spo2ss,30,30,1);
  display.setCursor(40,40);
  display.setTextSize(2);display.print(spo2);display.setTextSize(1);display.println("%");
  display.display();
  Serial.print("Heart rate:");
  Serial.print(heartbeat);
  Serial.print("bpm / SpO2:");
  Serial.print(spo2);
  Serial.println("%");
}

void getDHTandMq2(){
  for (int i = 0; i < 5; i++)
  {
    humiSS[i] = dht.readHumidity();
    tempSS[i] = dht.readTemperature();
    gasValSS[i] = analogRead(MQ2pin);
    delay(30);
  }
  gasSS = (gasValSS[0] + gasValSS[1] + gasValSS[2] + gasValSS[3] + gasValSS[4]) / 5;
  humiditySS = (humiSS[0] + humiSS[1] + humiSS[2] + humiSS[3] + humiSS[4]) / 5;
  temperatureSS = (tempSS[0] + tempSS[1] + tempSS[2] + tempSS[3] + tempSS[4]) / 5;
}

  void drawDHT11andMq2(){
    getDHTandMq2();
    display.clearDisplay();
    // drawText("Humidity: ",0,0,false,1);
    display.drawBitmap(3,0,humss,30,40,1);
    display.setCursor(0,45);
    display.print(humiditySS);display.println("%");
    // drawText("TemperatureSS: ",0,10,false,1);
    display.drawBitmap(45,0,tempss,30,40,1);
    display.setCursor(48,45);
    display.print(temperatureSS);display.println("C");
    // drawText("Air: ",0,20,false,1);
    display.drawBitmap(95,0,gasss,30,30,1);
    display.setCursor(95,45);
    display.print(gasSS);
    display.display();
    if(gasSS > 800){
      for(int i = 0; i < 10 ; i++){
        ledcWrite(channel, 0); // Bật còi
        delay(1000);
      }
    }
  }

void drawDigitalClock(){
  setLocalTime();
  display.clearDisplay();
  display.setCursor(35,10);
  display.print(daymonyear);
  display.setCursor(20,25);
  display.setTextSize(2);
  display.print(hms);
  drawText("TP.THU DUC",35,45,false,1);
  display.display();
  delay(50);
}

void uploadFirebase(){
  Firebase.setFloat(fbdo,"/DA1/Temp",temperatureSS);
  Firebase.setFloat(fbdo,"/DA1/Hum",humiditySS);
  Firebase.setFloat(fbdo,"/DA1/Gas",gasSS);
  display.drawBitmap(115,45,checked,10,10,1);
  display.display();
  delay(200);
  button_state = digitalRead(BUTTON);
  while(1){
    if(button_state == HIGH){
      delay(50);
    button_state = digitalRead(BUTTON);
    if(button_state == LOW){
      count = 5;
      break;
      }
    }
    else 
      break;
  }
}



