/*************************************************************
  Wemos Lolin D32 PRO (ESP32)
  Sensorless Weather Station (version 2.0)
  (see version 1.0 here: https://gist.github.com/xxlukas42/3085c795999c78926bf8b1055d1476f7)
  by Petr Lukas
*************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <SD.h>

// Adafruit GFX libraries
#include <Adafruit_GFX.h>
#include <Adafruit_ImageReader.h>
#include <Adafruit_ILI9341.h>

// Touchscreen library
#include <XPT2046_Touchscreen.h>

// Fonts
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/OpenSansBold9pt7b.h>
#include <Fonts/OpenSansBold14pt7b.h>
#include <Fonts/OpenSansBold30pt7b.h>

// Default pin numbers for D32 Pro
#define TFT_CS  14
#define TFT_DC  27
#define TFT_RST 33
#define TS_CS   12

// SD card pin
#define SD_CS    4

// WIFI
const char* ssid = "YOUR_SSID";
const char* password =  "YOUR_PASSWORD";

// OPENWEATHER API CONFIGURATION
String city =  "brno,cz";
String key = "YOUR_API_KEY";

// MAIN SETTINGS
// Screen (0 = current weather, 1 = settings, 2 = weather forecast)
byte screen = 0;
// Time zone
int timezone = 1;
// Wifi bar (0 = disabled, 1 = enabled)
bool bar = 1;
// Temperature unit (0 = Celsius, 1 = Fahrenheit, 2 = Kelvin);
byte preset_unit = 0;
// 24H/12H time format (0 = 24H, 1 = 12H)
byte time_format = 0;

// Refresh time (0 = 1 mins, 1 = 5 mins, 2 = 15 mins)
int refresh_time = 0; 

// Clock-watcher
long previousMillis = 0; 
String payload;

// Colors
#define ILI9341_GRAY 0x734D
#define ILI9341_BLUE 0x04D5

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_ImageReader reader;
XPT2046_Touchscreen ts(TS_CS);
 
void setup() {
 
  Serial.begin(115200);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); 

  tft.begin();
  tft.setFont(&FreeMono9pt7b);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(0, 38);

  tft.println("Init SD card...");
  if (!SD.begin(SD_CS)) {
    tft.setTextColor(ILI9341_RED);
    Serial.println("SD card failed");
    tft.println("Failed!");
    delay(999999);
    return;
  }
  Serial.println("OK");
  tft.println("OK");
    
  tft.println("Init touchscreen...");
  ts.begin();
 
  connectWifi();
  
  if(WiFi.status() == WL_CONNECTED) tft.println("\nConnected");
  
  delay(1000);
  
  tft.fillScreen(ILI9341_BLACK);
  printWifi();

  // Get first weather data
  getCurrentWeather();
}
 
void loop() {
  if(WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(ILI9341_BLACK);
    printWifi();
    tft.setFont(&FreeMono9pt7b);
    tft.setTextColor(ILI9341_RED);
    tft.println("Unable to connect!");
    delay(10000);
    tft.fillScreen(ILI9341_BLACK);
    connectWifi();
    delay(10000);
  }
   
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // You need to test coordinates (in this case landscape oriented)
    /*
    tft.setCursor(0,50);
    tft.fillRect(0,30,100,50,ILI9341_BLACK);
    tft.println(p.x);
    tft.println(p.y);
    */

    switch(screen){
      case 0: 
        if(p.x > 3000){
          tft.fillScreen(ILI9341_BLACK);
          screen = 1;
          showSettings(); 
        }
        break;
      case 1: 
        if(p.x < 700){
          tft.fillScreen(ILI9341_BLACK);
          screen = 0;
          getCurrentWeather();  
        }

        if((p.x < 3200 && p.x > 2700) && (p.y < 900)){
          tft.fillScreen(ILI9341_BLACK);
          timezone++;
          if(timezone > 12) timezone = 0;
          showSettings();
        }

        if((p.x < 3200 && p.x > 2700) && (p.y < 2000 && p.y > 1500)){
          tft.fillScreen(ILI9341_BLACK);
          timezone--;
          if(timezone < -12) timezone = 0;
          showSettings();
        }

        if(p.x < 1100 && p.x > 700){
          tft.fillScreen(ILI9341_BLACK);
          if(bar) bar = 0;
          else bar = 1;
          showSettings();
        }
        
        // Button for temperature unit change
        if(p.x < 2600 && p.x > 2100){
          tft.fillScreen(ILI9341_BLACK);
          preset_unit++;
          if(preset_unit > 2) preset_unit = 0;
          showSettings();
        }

        // Button for refresh time change
        if(p.x < 2100 && p.x > 1700){
          tft.fillScreen(ILI9341_BLACK);
          refresh_time++;
          if(refresh_time > 2) refresh_time = 0;
          showSettings();
        }

        // Button for time format
        if(p.x < 1700 && p.x > 1100){
          tft.fillScreen(ILI9341_BLACK);
          time_format++;
          if(time_format > 1) time_format = 0;
          showSettings();
        }
      break;
      case 2: break;
    }
    delay(500);
  }
     
  unsigned long currentMillis = millis();
  int refresh;
  switch(refresh_time){
    case 0: {
      refresh = 1;
    }
    break;
    case 1: {
      refresh = 5;
    }
    break;
    case 2: {
      refresh = 15;
    }
    break;
  }

  if(currentMillis - previousMillis > (refresh * 60000) && screen == 0) {
    previousMillis = currentMillis; 
    getCurrentWeather();
  }
}

void connectWifi(){
  Serial.println(WiFi.status());
  Serial.println("Starting wifi");
  tft.println("Starting wifi");
  WiFi.begin(ssid, password);

  tft.print("Connecting");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {  
    if(timeout > 10){
      tft.setTextColor(ILI9341_RED);
      tft.setCursor(0, 38);
      tft.println("\nConnection timeout!");
      return;
    }
    tft.print(".");
    timeout++;
    delay(1000);
  }
  if(WiFi.status() == WL_CONNECTED) Serial.println("Connected"); 
}

// Get weather data in JSON from OpenWeatherMap API
void getCurrentWeather(){
  // Setup request string;
  String temperature_unit;
  char* temperature_deg;
  
  switch(preset_unit){
    case 0: {
      temperature_unit = "metric";
      temperature_deg = "C";
    }
    break;
    case 1: {
      temperature_unit = "imperial";
      temperature_deg = "F"; 
    }
    break;
    case 2: {
      temperature_unit = "default";
      temperature_deg = "K"; 
    }
    break;
  }

  String current = "http://api.openweathermap.org/data/2.5/weather?q="+city+"&units="+temperature_unit+"&APPID="+key;
  printWifi();
  Serial.print("Getting current weather");
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;
 
    http.begin(current); //Specify the URL
    int httpCode = http.GET();  //Make the request

    String payload;
    if (httpCode > 0) { //Check for the returning code
      const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 600;
      DynamicJsonDocument doc(capacity);
      
      String payload = http.getString();
      //Serial.println(httpCode);
      //Serial.println(payload);

      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);

      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }
    
    ImageReturnCode stat;

    int id = doc["weather"][0]["id"];
    Serial.println(id);
    
    float temperature = doc["main"]["temp"];
    int humidity = doc["main"]["humidity"];
    int pressure = doc["main"]["pressure"];
    float visibility = doc["visibility"];
    String description = doc["weather"][0]["description"];
    float wind_speed = doc["wind"]["speed"];
    int wind_deg = doc["wind"]["deg"];
    int rain_volume = doc["rain"]["1h"];
    int snow_volume = doc["snow"]["1h"];

    Serial.print(convertTimestamp(doc["dt"],time_format));
    //int id = 700;
    bool is_night = true;
    
    // Weather image
    char icon[28];
    
    if(id < 233) char icon[] = "/weather/wi-thunderstorm.bmp";
    if(id > 299 && id < 322) strncpy(icon, "/weather/wi-sleet.bmp", 28);
    if(id > 499 && id < 502) strncpy(icon, "/weather/wi-showers.bmp", 28);
    if(id > 501 && id < 532) strncpy(icon, "/weather/wi-rain.bmp", 28);
    if(id > 599 && id < 623) strncpy(icon, "/weather/wi-snow.bmp", 28);
    if(id > 699 && id < 782) strncpy(icon, "/weather/wi-fog.bmp", 28);
    
    // Different icon for night and day
    bool isDay = ((intTime(doc["dt"]) >= intTime(doc["sys"]["sunrise"])) && (intTime(doc["dt"]) <= intTime(doc["sys"]["sunset"])));
    
    if(id == 800 && isDay) strncpy(icon, "/weather/wi-day-sunny.bmp", 28);
    if(id == 800 && !isDay) strncpy(icon, "/weather/wi-stars.bmp", 28);
    
    if(id > 800 && id < 803) strncpy(icon, "/weather/wi-day-cloudy.bmp", 28);
    if(id > 802) strncpy(icon, "/weather/wi-cloud.bmp", 28);

    tft.fillRect(0, 20, 120, 105, ILI9341_BLACK);
    stat = reader.drawBMP(icon, tft, 10, 26);
   
    //tft.setFont(&OpenSansBold9pt7b);
    //tft.setCursor(10, 136);
    //tft.fillRect(0, 128, 232, 25, ILI9341_BLACK);
    //tft.print(description);

    tft.setFont(&OpenSansBold14pt7b);
    tft.setCursor(18, 172);
    tft.print(temperature_deg);
    tft.setFont(&OpenSansBold9pt7b);
    tft.setCursor(8, 162);
    tft.print("o");

    // Main temperature
    tft.setFont(&OpenSansBold30pt7b);
    tft.fillRect(37, 150, 203, 52, ILI9341_BLACK);
    tft.setCursor(40, 195);
    
    temperature = temperature*10;
    temperature = round(temperature);
    tft.print(temperature/10,1);
    
    tft.setFont(&OpenSansBold9pt7b);

    tft.fillRect(50, 235, 268, 45, ILI9341_BLACK);
    stat = reader.drawBMP("/wi-strong-wind.bmp", tft, 5, 235);
    
    String direction = "Northerly";
    if(wind_deg > 22.5) direction = "North Easterly";
    if(wind_deg > 67.5) direction =  "Easterly";
    if(wind_deg > 122.5) direction =  "South Easterly";
    if(wind_deg > 157.5) direction =  "Southerly";
    if(wind_deg > 202.5) direction =  "South Westerly";
    if(wind_deg > 247.5) direction =  "Westerly";
    if(wind_deg > 292.5) direction =  "North Westerly";
    if(wind_deg > 337.5) direction =  "Northerly";
   
    tft.setCursor(50, 252);
    tft.print(direction);
    
    tft.setCursor(50, 275);
    tft.print(wind_speed,1);
    tft.print(" km/h");
    
    int y = 20;

    
    printRow(120, 25, visibility/1000, "/wi-dust.bmp", "km",1);
    printRow(120, 57, humidity, "/wi-humidity.bmp", "%",0);
    printRow(120, 90, pressure, "/wi-barometer.bmp", "hPa",0);
    
    tft.setTextColor(ILI9341_BLUE);
    printRow(0, 198, rain_volume, "/wi-raindrops.bmp", "mm",0);
    printRow(120, 198, snow_volume, "/wi-snowflake-cold.bmp", "mm",0);
    tft.setTextColor(ILI9341_WHITE);
       
    printRow(10, 283, -1, "/wi-sunrise.bmp", doc["sys"]["sunrise"], 0);
    printRow(130, 283, -1, "/wi-sunset.bmp", doc["sys"]["sunset"], 0);
    
    } else {
      Serial.println("HTTP request error");
    }
    http.end();
  }
}

// Print weather data with icon, hide if not available
void printRow(int x, int y, float volume, char *icon, String unit, int decimal){
  
  tft.fillRect(x, y+5, 130, 25, ILI9341_BLACK);
  
  if(volume == 0) return; 
  
  ImageReturnCode stat;
  stat = reader.drawBMP(icon, tft, x, y);
  
  tft.setFont(&OpenSansBold9pt7b);
  
  // Special case for sunrise and sunset
  if(icon == "/wi-sunrise.bmp" || icon == "/wi-sunset.bmp"){
    unit = convertTimestamp(unit,time_format);
    tft.setCursor(x+42,y+25);
    tft.print(unit);
    return;
  }

  
  tft.setCursor(x+37,y+25);
  tft.print(volume,decimal);
  tft.print(" ");
  tft.println(unit);
}

void showSettings(){
  tft.fillScreen(ILI9341_BLACK);
  printWifi();
  
  tft.setFont(&OpenSansBold9pt7b);

  // Signal level
  tft.setCursor(10,50);
  tft.print("Signal:");
  tft.setCursor(150,50);
  tft.print(WiFi.RSSI());
  tft.print(" dBm");

  // Change time zone
  tft.setCursor(10,90);
  tft.print("Time zone:");

  // Minus button 
  tft.drawLine(136, 85, 149, 85, ILI9341_WHITE);
  tft.drawCircle(142, 85, 18, ILI9341_WHITE);

  tft.setCursor(170,90);
  tft.print(timezone);
  
  // Plus button 
  tft.drawLine(207, 85, 219, 85, ILI9341_WHITE);
  tft.drawLine(213, 79, 213, 91, ILI9341_WHITE);
  tft.drawCircle(213, 85, 18, ILI9341_WHITE);

  // Row buttons
  int rstart = 115;
  for(byte r=1; r<5; r++){
    tft.drawLine(0, rstart, 240, rstart, ILI9341_WHITE);
    rstart = rstart+40;
  }

  // Temperature unit settings
  String temperature_unit_name;
  switch(preset_unit){
    case 0: {
      temperature_unit_name = "Celsius";
      tft.drawLine(130, 143, 198, 143, ILI9341_BLUE); 
    }
    break;
    case 1: {
      temperature_unit_name = "Fahrenheit"; 
      tft.drawLine(130, 143, 230, 143, ILI9341_BLUE); 
    }
    break;
    case 2: {
      temperature_unit_name = "Kelvin"; 
      tft.drawLine(130, 143, 187, 143, ILI9341_BLUE);
    }
    break;
  }
  
  tft.setCursor(10,140);
  tft.print("Temp. unit:");
  tft.setTextColor(ILI9341_BLUE);
  tft.setCursor(130,140);
  tft.print(temperature_unit_name);
  
  tft.setTextColor(ILI9341_WHITE);
  
  // Refresh time
  tft.setCursor(10,180);
  tft.print("Refresh time:");
  tft.setCursor(140,180);
  tft.setTextColor(ILI9341_BLUE);

  switch(refresh_time){
    case 0:
      tft.print("60 sec"); 
      break;
    case 1:
      tft.print("5 min"); 
      break;
    case 2:
      tft.print("15 min"); 
      break;
  }
  tft.drawLine(140, 183, 188, 183, ILI9341_BLUE);
  
  tft.setTextColor(ILI9341_WHITE);

  // Time format
  tft.setCursor(10,220);
  tft.print("Time format:");
  tft.setCursor(140,220);
  tft.setTextColor(ILI9341_BLUE);

  if(time_format == 0){
    tft.print("24 H");
  }
  if(time_format == 1){
    tft.print("12 H");
  }
  tft.drawLine(140, 223, 178, 223, ILI9341_BLUE); 
  tft.setTextColor(ILI9341_WHITE);
 
  // Wifi bar
  tft.setCursor(10,270);
  tft.print("Wifi bar:");
  tft.drawRect(100, 255, 20, 20, ILI9341_WHITE);
  if(bar) tft.fillRect(105, 260, 10, 10, ILI9341_WHITE);
}

// Display signal level and SSID
void printWifi(){
  if(!bar) return;
  tft.setFont(&OpenSansBold9pt7b);
  tft.setTextColor(ILI9341_WHITE);
  tft.fillRect(0, 0, 240, 22, ILI9341_BLACK);
  
  tft.fillRect(221, 16, 4, 3, ILI9341_GRAY);
  tft.fillRect(226, 13, 4, 6, ILI9341_GRAY);
  tft.fillRect(231, 8, 4, 11, ILI9341_GRAY);
  tft.fillRect(236, 3, 4, 16, ILI9341_GRAY);
  
  ImageReturnCode stat; 
  
  if(WiFi.status() != WL_CONNECTED){
    stat = reader.drawBMP("/wifi1.bmp", tft, 0, 0);
    tft.setCursor(30, 16);
    tft.println("NO NETWORK");
    return;
  }

  //tft.drawLine(0, 20, 240, 20, ILI9341_GREEN);
  
  stat = reader.drawBMP("/wifi2.bmp", tft, 0, 0);
  tft.setCursor(30, 16);
  tft.print(WiFi.SSID());
  //tft.setCursor(160, 16);
  //tft.print(WiFi.RSSI());
 
  int ss = WiFi.RSSI();
 
  if(ss > -90) tft.fillRect(221, 16, 4, 3, ILI9341_WHITE);
  if(ss > -80) tft.fillRect(226, 13, 4, 6, ILI9341_WHITE);
  if(ss > -70) tft.fillRect(231, 8, 4, 11, ILI9341_WHITE);
  if(ss > -65) tft.fillRect(236, 3, 4, 16, ILI9341_WHITE);
}

// The esp products use the standard POSIX time library functions
String convertTimestamp(String timestamp, byte time_format){
  long int time_int = timestamp.toInt();
  time_int = time_int+timezone*3600;
  struct tm *info;
  
  time_t t = time_int;
  info = gmtime(&t);
  String result;
  byte hours = ((info->tm_hour) > 12 && time_format == 1)? info->tm_hour - 12 : info->tm_hour;
  
  String noon;
  if(time_format == 1){
    noon = ((info->tm_hour) > 12)? " PM" : " AM";
  } else {
    noon = "";
  }
  
  
  String o = (info->tm_min < 10)? "0":"";
  result = String(hours)+":"+o+String(info->tm_min)+noon;
  
  return result;
}

// Time to comparable integer to determine day/night
int intTime(String timestamp){
  long int time_int = timestamp.toInt();
  struct tm *info;
  time_t t = time_int;
  info = gmtime(&t);
  int result = info->tm_hour*100+info->tm_min;

  Serial.println(result);
  return result;
}
