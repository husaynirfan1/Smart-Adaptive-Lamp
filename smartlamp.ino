#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

RTC_DS3231 rtc;
char t[32];

float sensitivity = 0.185;
long prevmillis = 0;
//readjust  timing for 1 hour interval
long interval = 1000; //60000 for 1 minute
long onehour = 60000; //3600000 for a day
float accumulatedVal = 0;
long prevMillisHour = 0;
int measurementsCount = 0;
float averagePerHour;
float powerW;
float accumulatedDay = 0;
float valueExceed=0;
const int buzzer = 9;
long lastLow;

//Average power consumption = 70.653 wattHour
const int PIN_TO_SENSOR = 2;   // the pin that OUTPUT pin of sensor is connected to
int pinStateCurrent   = LOW; // current state of pin
int pinStatePrevious  = LOW; // previous state of pin
const int RELAY_PIN = 3;
bool condCheck;
unsigned long currmillis;

const int ledPin = 8;


void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);

 // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer.
  display.clearDisplay();

  // Display Text
  testdrawline();
  delay(2000);    // Draw many lines


  condCheck = false;
  pinMode(buzzer, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PIN_TO_SENSOR, INPUT);
  Wire.begin();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  rtc.clearAlarm(1);
  rtc.setAlarm1(DateTime(2020, 6, 3, 12, 0, 0), DS3231_A1_Hour); // setback to 12 am
  abnormalSys(false);
  //rtc.adjust(DateTime(2019, 1, 21, 5, 0, 0));
}
void loop() {
  DateTime now = rtc.now();
  sprintf(t, "%02d:%02d:%02d %02d/%02d/%02d", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());  
  Serial.print(F("Date/Time: "));
  Serial.println(t);
  delay(1000);

  pinStatePrevious = pinStateCurrent; // store old state
  pinStateCurrent = digitalRead(PIN_TO_SENSOR);   // read new state
          Serial.println(accumulatedDay);

  currmillis = millis();

  if(currmillis - prevMillisHour >= onehour){

    prevMillisHour = currmillis;
    if (measurementsCount > 0) {
          averagePerHour = accumulatedVal / measurementsCount;
          //accumulate for a day
          accumulatedDay+= averagePerHour;
          Serial.println(accumulatedDay);
          Serial.print("Watt-Hour(1Hour) : ");
          Serial.println(averagePerHour, 4);
    
    } else Serial.println("No valid readings for the past hour");

    accumulatedVal = 0;
    measurementsCount = 0;
    averagePerHour = 0;

  }
  delay(10);
  
  if(currmillis - prevmillis >= interval){
        int adc = analogRead(A0);
        float voltageOut = adc * 5 / 1023.0;
        float current = (voltageOut - 2.5) / sensitivity;

        if(current != 0 && current > 0.16){
            powerW = current * 5; 


            measurementsCount++;
            accumulatedVal+= powerW;
        }
  }
  // AT 12 AM RESET ACCU DAY VAL.
  

  Serial.print("Accumulated : ");
  Serial.println(accumulatedDay);

  if(accumulatedDay > 1.653){   // avg actual 70.563
    digitalWrite(RELAY_PIN, LOW);
    delay(1000);

    digitalWrite(ledPin, HIGH);
    delay(2000);
    digitalWrite(ledPin, LOW);

    abnormalSys(true);
    // Clear the buffer.
    display.clearDisplay();

  // Display Text
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30,2);
  display.println("Abnormal Mode");
  display.display();

  const float savedVal = accumulatedDay;
 float exceedVal = powerW * 0.017; // To get watt hour in watt minute value

  // Display Text
  display.setTextSize(0.5);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.print("Total(wH): ");
  display.println(accumulatedDay, 1);
  display.display();


    // Display Text
  display.setTextSize(0.5);
  display.setTextColor(WHITE);
  display.setCursor(0,17);
  display.print("Exceed(wH): ");
  display.println((accumulatedVal/measurementsCount), 1);
  display.display();

  } else {
    condCheck = false;
    // Clear the buffer.
  display.clearDisplay();

  // Display Text
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30,2);
  display.println("Normal Mode");
  display.display();

  // Display Text
  display.setTextSize(0.5);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.print("Total Power(wH): ");
  display.println(accumulatedDay, 1);

  display.display();
  }

  if(!condCheck){
    if(rtc.alarmFired(1) == 1){
    digitalWrite(RELAY_PIN, HIGH);
    //set again every 12AM
    rtc.clearAlarm(1);
    rtc.setAlarm1(DateTime(2020, 6, 25, 12, 0, 0), DS3231_A1_Hour); //set back to 12 am
    delay(500);
              } else {
    digitalWrite(RELAY_PIN, LOW);
    delay(500);
    }
  
  }

}

void abnormalSys(boolean check){
  if(check == true){
    condCheck = true;
    Serial.println("Abnormal Mode on");
    tone(buzzer, 1000);
    
   
    if (pinStatePrevious == LOW && pinStateCurrent == HIGH) {   // pin state change: LOW -> HIGH
    Serial.println("Motion detected!");
    digitalWrite(RELAY_PIN, LOW);
    delay(500);
    digitalWrite(ledPin, HIGH);
    //TODO 
    valueExceed+= powerW;
    
    

    //digitalWrite(RELAY_PIN, LOW);
  //delay(500);
    // TODO: turn on alarm, light or activate a device ... here


  }
  else if (pinStatePrevious == HIGH && pinStateCurrent == LOW) {   // pin state change: HIGH -> LOW
    Serial.println("Motion stopped!");
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
    lastLow = millis();
    Serial.print("LAstlow = ");
    Serial.println(lastLow);
    Serial.println("begintimer10secs");
    digitalWrite(ledPin, LOW);
    
    if(currmillis - lastLow >= 10000){
    Serial.println("Abnormal mode off");
    check = false;
    accumulatedDay = 0;
    noTone(buzzer);
    }
   
    // TODO: turn off alarm, light or deactivate a device ... here
  }
   
   

  }
  
 
}
void testdrawline() {
  int16_t i;

  display.clearDisplay(); // Clear display buffer

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, SSD1306_WHITE);
    display.display(); // Update screen with each newly-drawn line
    delay(1);
  }
  for(i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  delay(2000); // Pause for 2 seconds
}
