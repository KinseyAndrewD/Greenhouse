#include <math.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define MISTER_PIN 4    // Output pin for Mister Relay (Relay 1)
#define FAN_PIN 5       // Output pin for Fan Relay (Relay 2)
#define WATER_PIN 6     // Output pin for Water Relay (Relay 3)
#define VENT_PIN 7      // Output pin for Vent Relay (Relay 4)

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//const int B = 4275000;          // B value of the thermistor
const int B = 4250;          // B value of the thermistor
const int R0 = 100000;          // R0 = 100k
const int pinTempSensor = A0;   // Grove - Temperature Sensor connect to A0

//initialize the library by associating any needed LCD interface pin
//with the arduino pin number it is connected to
const int rs = 13, en = 12, d4 = 11, d5 = 10, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//Initialize Moisture Sensor
int moisturePin = A1;
int moisture = 0;

//Water Valve Status
bool waterValve = false;    //Watering valve that opens when soil is dry
bool mistValve = false;     //Misting sprayer that turns on when temp exceeds value

//Timers
int waterTime = 0;  //Time watering valve has been open (in seconds)
int tempTime = 0;   //Time since temp last checked (in seconds)

void setup()
{
    pinMode(MISTER_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(WATER_PIN, OUTPUT);
    pinMode(VENT_PIN, OUTPUT);
    digitalWrite(MISTER_PIN,LOW);  //Relays are active high
    digitalWrite(FAN_PIN,LOW);
    digitalWrite(WATER_PIN,LOW);
    digitalWrite(VENT_PIN,LOW);

    //Wire.begin(SCREEN_ADDRESS);  // Needed for some older boards
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.setTextWrap(false);
    Serial.begin(9600);
    lcd.begin(16, 2);
    display.display();

    display.clearDisplay();     // Clear the buffer
    display.setTextSize(1);     //Size 1 = 8 rows of text, size 2 = 4 rows of text
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);        //Use proper Code Page 437 codes

    //Print a message to the LCD.
    lcd.print("Temp:");
    lcd.setCursor(0,1);
    lcd.print("H20:");

    updateOLED(0,0,0.0,0);
    Serial.write("Finished Setup");
}

void loop()
{
    //Calculate Temperature
    int a = analogRead(pinTempSensor);
    float R = 1023.0/a-1.0;
    R = R0*R;
    float temperature = 1.8*(1.0/(log(R/R0)/B+1/298.15)-273.15)+32; // convert to temperature (F)
    //float temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to temperature (C)

    //Get Moisture
    moisture = analogRead(moisturePin);

    // Moisture < 300 is dry soil and make sure valve is currently closed

    if (moisture < 300 && waterTime == 0 && !waterValve) {
        //turn on water
        waterValve = true;
        digitalWrite(WATER_PIN, HIGH);   // Relays are active high
        //Water should run for 3 hours (10,800 seconds)
        waterTime = 10800;
        //waterTime = 5;  //Debug - 5 seconds for testing

    } else if (waterTime == 0 && waterValve) {
        //Turn off water
        waterValve = false;
        digitalWrite(WATER_PIN, LOW);  // Relays are active high
        waterTime = 21600;              //Do not check again for 6 hours (21,600 seconds)

        //Update Water Valve Status
        lcd.setCursor(8,1);
        lcd.print("Closed");
    }

    // TODO: Handle Temp Sensor
    // If temp > 85, open vent
    // If temp > 90, turn on fan
    // If temp > 95, turn on misters (do this first)
    // Only check every 30 minutes

    if (tempTime == 0) {
        //for testing, change to 70
        if (temperature < 85.0) {
            digitalWrite(VENT_PIN, LOW);
            digitalWrite(MISTER_PIN, LOW);
            digitalWrite(FAN_PIN, LOW);

            mistValve = false;
        }
        else if (temperature < 90.0) {
            digitalWrite(VENT_PIN, HIGH);
            digitalWrite(FAN_PIN, LOW);
            digitalWrite(MISTER_PIN, LOW);

            mistValve = false;
        }
        else if (temperature < 95.0) {
            digitalWrite(VENT_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);
            digitalWrite(MISTER_PIN, LOW);

            mistValve = false;
        }
        else {
            digitalWrite(VENT_PIN, HIGH);
            digitalWrite(MISTER_PIN, HIGH);
            digitalWrite(FAN_PIN, HIGH);

            mistValve = true;
        }
    }
  
    updateOLED(mistValve, waterValve, temperature, moisture);

    lcd.setCursor(5,0);  //First row, 6th space
    lcd.print(temperature);

    lcd.setCursor(4,1);  //Second row, 5th space
    lcd.print(moisture);

    if (waterTime > 0) {                
        displayTime(8,1,waterTime);
        waterTime = waterTime-1; // Loop runs approximately once per second
    }
    if (tempTime > 0) {
        tempTime = tempTime-1;
    } else { 
        tempTime = 1800;
    }
    delay(1000);
}

/*  
    Display the time in hours, minutes, and seconds to LCD
    Takes three arguments:
    line = line of LCD
    column = column of LCD
    timeVal = time to convert (in seconds)
*/
void displayTime (int column, int line, int timeVal) {
    
    int hours = (timeVal / 60) / 60;
    int minutes = (timeVal /60) % 60;
    int seconds = (timeVal % 60) % 60;

    lcd.setCursor(column,line);
    lcd.print(hours);
    lcd.print(":");
    lcd.print(minutes);
    lcd.print(":");
    lcd.print(seconds);
}

/* 
    Updates the OLED Display

    Inputs
    mister = 0 if misters are closed, 1 if open
    water = 0 if water valve is closed, 1 if open
    temp = temperature
    moisture = soil moisture

*/
void updateOLED(bool mister, bool water, float temp, int moisture) {
    
    display.clearDisplay();

    String tempStr = "Temperature:";
    String moistureStr = "Soil Moisture:";
    String misters = "Misters: ";
    String watervalve = "Water Valve: ";

    tempStr.concat(temp);
    moistureStr.concat(moisture);

    if (mister) {
        misters.concat("Open");
    } else {
        misters.concat("Closed");
    }

    if (water) {
        watervalve.concat("Open");
    } else {
        watervalve.concat("Closed");
    }

    display.setCursor(0, 0);        //First line is temperature
    display.print(tempStr);         //Print Temperature
    display.write(0xF8);            //Add degree symbol
    display.setCursor(0,8);         //Second line is Misting Valve
    display.print(misters);
    display.setCursor(0,16);        //Third line is soil moisture
    display.print(moistureStr);     //Print Moisture
    display.setCursor(0,24);        //Fourth line is Watering Valve
    display.print(watervalve);
    
    display.display();
}
