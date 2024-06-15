// Defines the PIN used.
#define PulseSensor_PIN 36 

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <PulseSensorPlayground.h>
#include <Arduino.h>
//----------------------------------------

// Include the web code (HTML, javascript and others) inside the char array variable for the web server page.
#include "PageIndex.h"

//----------------------------------------

const char* ssid = "ESP32_WS";  //--> access point name
const char* password = "99999999"; //--> access point password

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);


unsigned long previousMillisGetHB = 0; //--> will store the last time Millis (to get Heartbeat) was updated.
unsigned long previousMillisResultHB = 0; //--> will store the last time Millis (to get BPM) was updated.

const long intervalGetHB = 35; //--> Interval for reading heart rate (Heartbeat) = 35ms.
const long intervalResultHB = 1000; //--> The reading interval for the result of the Heart Rate calculation.

int timer_Get_BPM = 0;

int PulseSensorSignal; //--> Variable to accommodate the signal value from the sensor.
int UpperThreshold = 705; //--> Determine which Signal to "count as a beat", and which to ignore.
int LowerThreshold = 685; 

int cntHB = 0; //--> Variable for counting the number of heartbeats.
boolean ThresholdStat = true; //--> Variable for triggers in calculating heartbeats.
int BPMval = 0; //--> Variable to hold the result of heartbeats calculation.

int x=0; //--> Variable axis x graph values to display on OLED
int y=0; //--> Variable axis y graph values to display on OLED
int lastx=0; //--> The graph's last x axis variable value to display on the OLED
int lasty=0; //--> The graph's last y axis variable value to display on the OLED

// Boolean variables to start and stop getting BPM values.
bool get_BPM = false;

// Variables for the "timestamp" of the BPM reading.
byte tSecond = 0;
byte tMinute = 0;
byte tHour   = 0;

// The char array variable to hold the "timestamp" data that will be sent to the web server.
char tTime[10];

//----------------------------------------

// The variables used to check the parameters passed in the URL.
// Look in the "PageIndex.h" file.
// xhr.open("GET", "BTN_Comd?BTN_Start_Get_BPM=" + state, true);
// For example :
// BTN_Comd?BTN_Start_Get_BPM=START
// PARAM_INPUT_1 = START
const char* PARAM_INPUT_1 = "BTN_Start_Get_BPM";

// Variable declaration to hold data from the web server.
String BTN_Start_Get_BPM = "";

// Initialize JSONVar
JSONVar JSON_All_Data;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Filter Variables
const int filterWindowSize = 10;  // Size of the moving average window
int filterWindow[filterWindowSize]; // Array to store signal values for the moving average filter
int filterIndex = 0; // Current index in the filter window
long filterSum = 0; // Sum of the values in the filter window
int filteredSignal = 0; // Filtered signal value

//________________________________________________________________________________void GetHeartRate()
void GetHeartRate() {
  //----------------------------------------Process of reading heart beat.
  unsigned long currentMillisGetHB = millis();

  if (currentMillisGetHB - previousMillisGetHB >= intervalGetHB) {
    previousMillisGetHB = currentMillisGetHB;

    // Read the PulseSensor's value and apply the moving average filter
    PulseSensorSignal = analogRead(PulseSensor_PIN); //--> Read the PulseSensor's value.

    filterSum -= filterWindow[filterIndex]; // Subtract the oldest value from the sum
    filterWindow[filterIndex] = PulseSensorSignal; // Add the new value to the window
    filterSum += PulseSensorSignal; // Add the new value to the sum
    filterIndex = (filterIndex + 1) % filterWindowSize; // Move to the next index
    filteredSignal = filterSum / filterWindowSize; // Calculate the average

    if (filteredSignal > UpperThreshold && ThresholdStat == true) {
      if (get_BPM == true) cntHB++;
      ThresholdStat = false;
    }

    if (filteredSignal < LowerThreshold) {
      ThresholdStat = true;
    }
    

    // Enter the data into JSONVar(JSON_All_Data).
    JSON_All_Data["heartbeat_Signal"] = filteredSignal;
    JSON_All_Data["BPM_TimeStamp"] = tTime;
    JSON_All_Data["BPM_Val"] = BPMval;
    JSON_All_Data["BPM_State"] = get_BPM;

    // Create a JSON String to hold all data.
    String JSON_All_Data_Send = JSON.stringify(JSON_All_Data);

    // Send Event to browser with JSON_All_Data Every 35 milliseconds.
    events.send(JSON_All_Data_Send.c_str(), "allDataJSON" , millis());
  }
  //----------------------------------------

  //----------------------------------------The process for getting the BPM value.
  // This timer or Millis is for reading the heart rate and calculating it to get the BPM value.
  // To get the BPM value based on the heart beat reading for 10 seconds.
  
  unsigned long currentMillisResultHB = millis();

  if (currentMillisResultHB - previousMillisResultHB >= intervalResultHB) {
    previousMillisResultHB = currentMillisResultHB;

    if (get_BPM == true) {
      timer_Get_BPM++;
      // "timer_Get_BPM > 10" means taking the number of heartbeats for 10 seconds.
      if (timer_Get_BPM > 10) {
        timer_Get_BPM = 1;

        tSecond += 10;
        if (tSecond >= 60) {
          tSecond = 0;
          tMinute += 1;
        }
        if (tMinute >= 60) {
          tMinute = 0;
          tHour += 1;
        }

        sprintf(tTime, "%02d:%02d:%02d",  tHour, tMinute, tSecond);

        BPMval = cntHB * 6; //--> The taken heart rate is for 10 seconds. So to get the BPM value, the total heart rate in 10 seconds x 6.
        Serial.print("BPM : ");
        Serial.println(BPMval);
        
        cntHB = 0;
      }
    }
  }
  //----------------------------------------
}

//________________________________________________________________________________VOID SETUP()
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //--> Set's up Serial Communication at certain speed.
  Serial.println();
  delay(2000);

  analogReadResolution(10);

  sprintf(tTime, "%02d:%02d:%02d",  tHour, tMinute, tSecond);
  //----------------------------------------
  //---------------------------------------- Set Wifi to AP mode
  Serial.println();
  Serial.println("-------------");
  Serial.println("WIFI mode : AP");
  WiFi.mode(WIFI_AP);
  Serial.println("-------------");
  //---------------------------------------- 

  delay(100);

  //---------------------------------------- Setting up ESP32 to be an Access Point.
  Serial.println();
  Serial.println("-------------");
  Serial.println("Setting up ESP32 to be an Access Point.");
  WiFi.softAP(ssid, password); //--> Creating Access Points
  delay(1000);
  Serial.println("Setting up ESP32 softAPConfig.");
  WiFi.softAPConfig(local_ip, gateway, subnet);
  Serial.println("-------------");
  //----------------------------------------


  delay(500);
  //---------------------------------------- Handle Web Server
  Serial.println();
  Serial.println("Setting Up the Main Page on the Server.");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", MAIN_page);
  });
  //----------------------------------------

  //---------------------------------------- Handle Web Server Events
  Serial.println();
  Serial.println("Setting up event sources on the Server.");
  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 10 second
    client->send("hello!", NULL, millis(), 10000);
  });
  //----------------------------------------

  //---------------------------------------- Send a GET request to <ESP_IP>/BTN_Comd?BTN_Start_Get_BPM=<inputMessage1>
  server.on("/BTN_Comd", HTTP_GET, [] (AsyncWebServerRequest * request) {
    //::::::::::::::::::
    // GET input value on <ESP_IP>/BTN_Comd?BTN_Start_Get_BPM=<inputMessage1>
    // PARAM_INPUT_1 = inputMessage1
    // BTN_Tare = PARAM_INPUT_1
    //::::::::::::::::::

    if (request->hasParam(PARAM_INPUT_1)) {
      BTN_Start_Get_BPM = request->getParam(PARAM_INPUT_1)->value();

      Serial.println();
      Serial.print("BTN_Start_Get_BPM : ");
      Serial.println(BTN_Start_Get_BPM);
    }
    else {
      BTN_Start_Get_BPM = "No message";
      Serial.println();
      Serial.print("BTN_Start_Get_BPM : ");
      Serial.println(BTN_Start_Get_BPM);
    }
    request->send(200, "text/plain", "OK");
  });
  //----------------------------------------

  //---------------------------------------- Adding event sources on the Server.
  Serial.println();
  Serial.println("Adding event sources on the Server.");
  server.addHandler(&events);
  //----------------------------------------

  //---------------------------------------- Starting the Server.
  Serial.println();
  Serial.println("Starting the Server.");
  server.begin();
  //---------------------------------------- 

  Serial.println();
  Serial.println("------------");
  Serial.print("SSID name : ");
  Serial.println(ssid);
  Serial.print("IP address : ");
  Serial.println(WiFi.softAPIP());
  Serial.println();
  Serial.println("Connect your computer or mobile Wifi to the SSID above.");
  Serial.println("Visit the IP Address above in your browser to open the main page.");
  Serial.println("------------");
  Serial.println();
}

//________________________________________________________________________________VOID LOOP()
void loop() {
  // put your main code here, to run repeatedly:

  if (BTN_Start_Get_BPM == "START" || BTN_Start_Get_BPM == "STOP") {
    delay(100);

    BTN_Start_Get_BPM = "";
    cntHB = 0;
    BPMval = 0;
    x = 0;
    y = 0;
    lastx = 0;
    lasty = 0;

    tSecond = 0;
    tMinute = 0;
    tHour   = 0;

    sprintf(tTime, "%02d:%02d:%02d",  tHour, tMinute, tSecond);

    get_BPM = !get_BPM;

    }
  GetHeartRate(); //--> Calling the GetHeartRate() subroutine.
}
