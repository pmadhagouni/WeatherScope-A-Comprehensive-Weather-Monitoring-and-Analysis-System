#include "MQ135.h"
#include "DHT.h"
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include "ThingSpeak.h"

/*Initializing variables to be used to read from the sesnors*/
int air_quality;
float Temperature;
float Humidity;

/*Defining SSID and Password to connect to the WIFI Network*/
const char* ssid     = "1237";          
const char* password = "OuR00m@1237";  
WiFiServer server(80); // Set web server port number to 80
WiFiClient client;

//-----ThingSpeak channel details
unsigned long myChannelNumber = 1; //Channel Number
const char * myWriteAPIKey = "0OTS166T048W4COX"; // Channel API Keys

/*pin used for the lcd module display*/
#define SDAPin 21
#define SCLPin 22
// set LCD address, number of columns and rows
LiquidCrystal_I2C lcd(0x27, 16, 2);  

/*defining to initialize the DHT-22 sensor*/
#define DHTTYPE DHT11   // DHT 11
uint8_t DHTPin = 4; // Pin used for DHT Sensor
DHT dht(DHTPin, DHTTYPE); // Initialize DHT sensor.               
long light = 0;

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(12, OUTPUT);          //Buzzer will be output to ESP32  
  pinMode(34, INPUT);           //Gas sensor will be an input to the ESP32
  pinMode(35,INPUT);            //lightdiode bpw34 connected to pin 35
  pinMode(DHTPin, INPUT);       //Reads Input from DHTPin 4
  Serial.print("Connecting to ");//connects esp32 to wifi and displays connection status and network IP address in serial monitor 
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  ThingSpeak.begin(client); // Initialize ThingSpeak
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  
  lcd.init();        //initialize LCD
  lcd.backlight();   // turn on LCD backlight   
  dht.begin();       //Initialize DHT
  
/*The following code is used to schedule the different tasks by setting different priorities; higher priority number means higher priority*/
/*The following is the algorithm for the FreeRTOS Prioritized Preemptive Scheduling with Time Slicing*/
/*Uses Round-Robin scheduling for tasks in the same priority; 8 total tasks to schedule*/
/*Function xTaskCreatePinnedToCore creates a new task with a specified affinity; similar to xTaskCreate, but allows setting the task affinity in SMP system*/
  xTaskCreatePinnedToCore(
    ReadTempHumid,        /*Function that is called*/
    "Sensors",            /*Name of the task*/
    10000,                /* Stack size (bytes)*/
    NULL,                 /*Parameter to pass*/
    3,                    /*Set task priority*/
    NULL,                 /*The task handle*/
    1                     /*The core of the ESP32 that is running*/
    );
  xTaskCreatePinnedToCore(ReadLight,"Light",10000,NULL,3,NULL,1);
  xTaskCreatePinnedToCore(Air_Quality,"Lux",10000,NULL,2,NULL,1);
  xTaskCreatePinnedToCore(LCD,"LCD",10000,NULL,1,NULL,1);
  /*Idle task is given a priority '0'*/
  xTaskCreatePinnedToCore(WIFI_Display,"Website",10000,NULL,0,NULL,1);   
}

/*adding the first task for the scheduler; first task is to read the DHT-22 sensor values*/
void ReadTempHumid(void * pvParameters){
  /*infinite loop*/ 
  for(;;){
    Serial.println("Reading the DHT Sensor: "); /*debugging purposes that checks if task is running by displaying onto serial monitor*/
    /*writing the function to read the values from the sensor*/
    Humidity = dht.readHumidity();
    Temperature = dht.readTemperature();
    delay(1000);
    Serial.println("Temp: ");
    Serial.println(Temperature);
    Serial.println("Humid: ");
    Serial.println(Humidity);
    /*adding delay to pause the task so that the scheduler can run other tasks*/
    vTaskDelay(2000);
  }
}

/*adding the second task for the scheduler; Second task is to read the Values from Photodiode i.e LUX*/
void ReadLight(void * pvParameters){
  for(;;){
    Serial.println("Task of Reading The Photodiode"); /*debugging purposes that checks if task is running by displaying onto serial monitor*/
    /*To read the values from the photodiode*/
    /*Based on the photodiode BPW34 datasheet, the following calculations are used to get the voltage value that will be used to get the lux units*/
    light += analogRead(35);
    int SensorValue = analogRead(35);
    delay(1000);
    Serial.println("Light: ");
    Serial.println(SensorValue);
  
    /*adding delay to pause the task so that the scheduler can run other tasks*/
    vTaskDelay(2000);
    
  }
}

/*This task gives indication through Buzeer if PPM levls go high*/
void Air_Quality (void * pvParameters){
  for(;;){
    MQ135 gasSensor = MQ135(34);
    float air_quality = gasSensor.getPPM()*1/100;
    if (air_quality >= 2000){
    digitalWrite(12, HIGH);   //Buzzer is ON
    delay(1000);
    }
    if (air_quality < 2000){
    digitalWrite(12, LOW);     //Buzzer is OFF
    delay(1000);
    }
    Serial.print(air_quality);      
    Serial.println(" PPM");
    /*adding delay to pause the task so that the scheduler can run other tasks*/
    vTaskDelay(2000);
  }
}

/*Output the sensor values into the LCD module*/
void LCD(void * pvParameters){
  /*infinite loop*/
  for(;;){
  // set cursor to first column, first row
  //Display Light on LCD
  lcd.setCursor(0, 0);
  lcd.print("TEAM 17");
  delay(3500);
  //Display Light on LCD
  lcd.setCursor(0, 0);
  lcd.print("light      =");
  lcd.setCursor(0, 1);
  lcd.print(light);
  delay(3500);
  //Display air quality on LCD
  lcd.setCursor(0, 0);
  lcd.print("Air Quality=");
  lcd.setCursor(0, 1);
  lcd.print(air_quality);
  delay(1500);
  //Display Temperature on LCD
  lcd.setCursor(0, 0);
  lcd.print("Temperature=");
  lcd.setCursor(0, 1);
  lcd.print(Temperature);
  delay(2500);
  //Display Humidity on LCD
  lcd.setCursor(0, 0);
  lcd.print("Humidity   =");
  lcd.setCursor(0, 1);
  lcd.print(Humidity);
  delay(3500);
  }
vTaskDelay(1000);
}

/*This Task sends data to Thingspeak*/
/*Allows us to write values to cloud*/
void WIFI_Display(void * pvParameters){
  for(;;){
    Serial.println("Task of website");  
    ThingSpeak.setField(2, Humidity);
    ThingSpeak.setField(1, Temperature);
    ThingSpeak.setField(3, light);
    ThingSpeak.setField(4, air_quality);  
    int x = ThingSpeak.writeFields(myChannelNumber,myWriteAPIKey);
    if(x == 200){
      Serial.println("Channel update successful.");
      }
  vTaskDelay(1000);
  }
}
void loop() {
/*
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
 
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  Serial.print(F("  Light Intensity : "));
  Serial.print(l);
  Serial.println(l1);

*/
vTaskDelay(1000);
}
