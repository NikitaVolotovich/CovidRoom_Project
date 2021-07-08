#include <GyverTimers.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#include <Wire.h>
#include "DFRobot_RGBLCD.h"
#include <Servo.h>
#include <SoftwareSerial.h>

#define OUTSIDE_LED_PIN 8
#define INSIDE_LED_PIN 3
#define BLUETOOTH_RX_PIN 4
#define BLUETOOTH_TX_PIN 5
#define EMERGENCY_BUTTON 2
#define MOTOR_PIN 6

#define ULTRASONIC_SENSOR_OUTSIDE_TRIG 9
#define ULTRASONIC_SENSOR_OUTSIDE_ECHO 10
#define ULTRASONIC_SENSOR_INSIDE_TRIG 11
#define ULTRASONIC_SENSOR_INSIDE_ECHO 12

#define AQ_SENSOR A2

#define NORMAL_MODE 0
#define BAD_AIR_MODE 1
#define BAD_COUNT_MODE 2
#define BAD_COUNT_AND_AIR_MODE 3
#define EMERGENCY_MODE 4

#define NORMAL_LED_MODE 0
#define ALARM_LED_MODE 1

#define NORMAL_AIR_QUALITY 300
#define MAXIMUM_AIR_QUALITY 450
#define MAXIMUM_PEOPLE 10
#define MAXIMUM_LED_BRIGHT 128
#define SENSOR_MISTAKE_PERCENT 0.20
#define DELAY_AFTER_DETECTING 100
#define BLINKING_PERIOD 10

#define DOOR_OPEN_STATE 500
#define DOOR_CLOSE_STATE 0
#define TIME_FOR_EXIT 1000

int mode = 0, LEDMode = 0;
int airQuality = 0;

int delayForDetecting = 25;
bool doDetecting = true;
float ultrasonicOutside = 0, ultrasonicInside = 0;
int ultrasonicSystematicErrorOutside, ultrasonicSystematicErrorInside;
int ultrasonicOutsideNormalDistance  = 0, ultrasonicInsideNormalDistance = 0;
int peopleCounter = 0, previousPeopleCounter = 0;

bool buttonIsPressed = false, doorIsOpen = false;
float distance, sensity;
int timeCounter = 0, timeForEmergencyExit = 0, blinkingLEDTimer = 0;

bool somethingWasDetected = false, blinkingNow = false;

int peopleCounterLEDRed = 0, peopleCounterLEDGreen = 0, peopleCounterLEDBlue = 0;
int AirQualityLEDRed = 0, AirQualityLEDGreen = 0, AirQualityLEDBlue = 0;
int LCDRed = 0,LCDGreen = 0, LCDBlue = 0;
int LCDTime = 0;

SoftwareSerial BTSerial(BLUETOOTH_RX_PIN, BLUETOOTH_TX_PIN);
Adafruit_NeoPixel outsideLED = Adafruit_NeoPixel(1, OUTSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel insideLED = Adafruit_NeoPixel(1, INSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Servo motor;
DFRobot_RGBLCD lcd(16,2);

void setup() {
  lcd.init();

  motor.attach(MOTOR_PIN);
  motor.write(0); 
  
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_ECHO, INPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_TRIG, OUTPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_ECHO, INPUT); 
  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_BUTTON), buttonInterrupt, FALLING);
  
  ultrasonicValueUpdate();
  
  while(ultrasonicOutsideNormalDistance == 0 && ultrasonicInsideNormalDistance == 0 ) {
    ultrasonicValueUpdate();
    ultrasonicOutsideNormalDistance = ultrasonicOutside;
    ultrasonicInsideNormalDistance  = ultrasonicInside;
  }

  ultrasonicSystematicErrorOutside = ultrasonicOutsideNormalDistance * SENSOR_MISTAKE_PERCENT;
  ultrasonicSystematicErrorInside = ultrasonicInsideNormalDistance * SENSOR_MISTAKE_PERCENT;
 
  outsideLED.begin();
  insideLED.begin();

  Serial.begin(115200);
  BTSerial.begin(115200);
  
  Timer2.setPeriod(100000);
  Timer2.enableISR(CHANNEL_A);

  Serial.print(" normal distance outside: ");
  Serial.println(ultrasonicOutsideNormalDistance);
  Serial.print(" normal distance inside: ");
  Serial.println(ultrasonicInsideNormalDistance);

  Serial.print("error outside cm: ");
  Serial.println(ultrasonicSystematicErrorOutside);
  Serial.print(" error inside cm: ");
  Serial.println(ultrasonicSystematicErrorInside);

  BTSerial.println("Initialization was successful");
}

void loop() {
  ultrasonicValueUpdate();
  detectPerson();
  updatePeripherals();
  modeSelector();
  modeExecutor();
  outputEverything();
}

void controlInsideLED() {
  insideLED.clear();   
  insideLED.setPixelColor(0, insideLED.Color(AirQualityLEDRed, AirQualityLEDGreen, AirQualityLEDBlue));
  insideLED.show();
}

void controlOutsideLED() {
  outsideLED.clear();  
  outsideLED.setPixelColor(0, outsideLED.Color(peopleCounterLEDRed, peopleCounterLEDGreen, peopleCounterLEDBlue));
  outsideLED.show();
}


void modeSelector() {
  if(airQuality < NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = NORMAL_MODE;
  } else if (airQuality > NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = BAD_AIR_MODE;
  } else if (airQuality < NORMAL_AIR_QUALITY && peopleCounter > MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = BAD_COUNT_MODE;
  } else if (airQuality > NORMAL_AIR_QUALITY && peopleCounter > MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = BAD_COUNT_AND_AIR_MODE;
  } else if (buttonIsPressed){
    mode = EMERGENCY_MODE;
  }
}


void modeExecutor() {
  switch (mode) {
    case NORMAL_MODE:
    LEDMode = NORMAL_LED_MODE;
    openDoor();
    break;
      
    case BAD_AIR_MODE:
    LEDMode = ALARM_LED_MODE;
    closeDoor();
    break;
      
    case BAD_COUNT_MODE:
    LEDMode = ALARM_LED_MODE;
    closeDoor();
    break;
      
    case BAD_COUNT_AND_AIR_MODE:
    LEDMode = ALARM_LED_MODE;
    closeDoor();
    break;
      
    case EMERGENCY_MODE:
    LEDMode = ALARM_LED_MODE;
    openDoor();
    break;
  } 
}

void updatePeripherals() {
  showOnLCD();
  colorRegulator();
  controlInsideLED();
  controlOutsideLED();
  outputEverything();
  outputEverythingToDashboard();
  airQuality = analogRead(AQ_SENSOR);

  if(timeForEmergencyExit > TIME_FOR_EXIT){
    timeForEmergencyExit = 0;
    buttonIsPressed = false;
    Serial.println("DOOR IS CLOSE NOW");
  }
}

void ultrasonicValueUpdate() {
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, HIGH);
  delayMicroseconds(10); 
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, LOW);

  long duration = pulseIn(ULTRASONIC_SENSOR_OUTSIDE_ECHO, HIGH);
  ultrasonicOutside = duration * 0.034 / 2;

  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, LOW);

  duration = pulseIn(ULTRASONIC_SENSOR_INSIDE_ECHO, HIGH);
  ultrasonicInside = duration * 0.034 / 2;
}

void detectPerson() {
  if(doDetecting){
    if(ultrasonicOutside < ultrasonicOutsideNormalDistance - ultrasonicSystematicErrorOutside && somethingWasDetected == false){
      Serial.println("OUTSIDE++");
      peopleCounter++;
      somethingWasDetected = true;
    }
    if(ultrasonicInside < ultrasonicInsideNormalDistance - ultrasonicSystematicErrorInside && somethingWasDetected == false){
      Serial.println("INSIDE--");
      if(peopleCounter != 0){
        peopleCounter--;
      }
      somethingWasDetected = true;
    }
    if(ultrasonicInside + ultrasonicSystematicErrorInside > ultrasonicInsideNormalDistance &&
    ultrasonicOutside + ultrasonicSystematicErrorOutside > ultrasonicOutsideNormalDistance &&
    ultrasonicInside < ultrasonicInsideNormalDistance + ultrasonicSystematicErrorInside &&
    ultrasonicOutside < ultrasonicOutsideNormalDistance + ultrasonicSystematicErrorOutside &&
    somethingWasDetected == true){
      Serial.print("BECOMES FALSE! INSIDE: ");
      Serial.print(ultrasonicInside);
      doDetecting = false;
      delayForDetecting = 0;
      somethingWasDetected = false;
    }
  }
}

void openDoor() {
  if(doorIsOpen == false){
    motor.write(DOOR_OPEN_STATE);
    doorIsOpen = true;
  }
}

void closeDoor() {
  if(doorIsOpen == true){
    motor.write(DOOR_CLOSE_STATE);
    doorIsOpen = false;
  }
}

void showOnLCD(){
  if(peopleCounter != previousPeopleCounter){
    lcd.clear();
  }
  previousPeopleCounter = peopleCounter;
  lcd.setRGB(LCDRed, LCDGreen, LCDBlue);    
  lcd.setCursor(0,0);
  
  lcd.print("Counter: " + String(peopleCounter));
  lcd.setCursor(0,1);
  lcd.print("Air: " + String(airQuality));
}

void colorRegulator(){
    switch(LEDMode){
      case NORMAL_LED_MODE:
        AirQualityLEDRed = MAXIMUM_LED_BRIGHT / ((float) MAXIMUM_AIR_QUALITY / airQuality);
        AirQualityLEDGreen = MAXIMUM_LED_BRIGHT - AirQualityLEDRed;
        if(peopleCounter == 0){
          peopleCounterLEDRed = 0;
          peopleCounterLEDGreen = MAXIMUM_LED_BRIGHT;
        } else {
          peopleCounterLEDRed = MAXIMUM_LED_BRIGHT / ((float) MAXIMUM_PEOPLE / peopleCounter);
          peopleCounterLEDGreen = MAXIMUM_LED_BRIGHT - peopleCounterLEDRed;
        }
        LCDRed = (abs(sin(3.14*LCDTime/180)))*255;       
        LCDGreen = (abs(sin(3.14*(LCDTime+60)/180)))*255;
        LCDBlue = (abs(sin(3.14*(LCDTime+120)/180)))*255;
        LCDTime += 3;
        break;
      case ALARM_LED_MODE:
        if(blinkingNow){
          AirQualityLEDRed = MAXIMUM_LED_BRIGHT;
          AirQualityLEDGreen = 0;
          peopleCounterLEDRed = MAXIMUM_LED_BRIGHT;
          peopleCounterLEDGreen = 0;
          LCDRed = 255;      
          LCDGreen = 0;
          LCDBlue = 0;
        } else if(!blinkingNow) {
          AirQualityLEDRed = 0;
          AirQualityLEDGreen = 0;
          peopleCounterLEDRed = 0;
          peopleCounterLEDGreen = 0;
          LCDRed = 0;      
          LCDGreen = 0;
          LCDBlue = 0;
        }
        break;
    }
}

void outputEverything() {
  Serial.print("Current mode: "+ String(mode) + "\n");
  Serial.print("Ultrasonic outside cm: " + String(ultrasonicOutside) + "\n");
  Serial.print("Ultrasonic inside cm: " + String(ultrasonicInside) + "\n");
  Serial.print("Button is pressed: " + String(buttonIsPressed) + "\n");
  Serial.print("Air quality: " + String(airQuality) + "\n");
  Serial.print("People counter: " + String(peopleCounter) + "\n");
  Serial.print("Time counter: " + String(timeCounter) + "\n");
}

void outputEverythingToDashboard() {
  BTSerial.print("M" + String(mode) + "*");
  BTSerial.print("O" + String(ultrasonicOutside) + "*");
  BTSerial.print("I" + String(ultrasonicInside) + "*");
  BTSerial.print("B" + String(buttonIsPressed) + "*");
  BTSerial.print("A" + String(airQuality) + "*");
  BTSerial.print("P" + String(peopleCounter) + "*");
  BTSerial.print("T" + String(timeCounter) + "*");
  BTSerial.print("CR" + String(LCDRed) + "G" + String(LCDGreen) + "B" + String(LCDBlue) + "*");
  BTSerial.print("QR" + String(AirQualityLEDRed) + "G" + String(AirQualityLEDGreen*2) + "B" + String(AirQualityLEDBlue) + "*");
  BTSerial.print("KR" + String(peopleCounterLEDRed) + "G" + String(peopleCounterLEDGreen) + "B" + String(peopleCounterLEDBlue) + "*");
  BTSerial.print("D" + String(ultrasonicOutsideNormalDistance) + "*");
  BTSerial.print("E" + String(ultrasonicInsideNormalDistance) + "*");
  BTSerial.print("Y" + String(ultrasonicSystematicErrorOutside) + "*");
  BTSerial.print("Z" + String(ultrasonicSystematicErrorInside) + "*");
  
  switch(mode){
    case NORMAL_MODE: 
      BTSerial.print("S JUST CHILL*");
      break;
    case BAD_AIR_MODE:
      BTSerial.print("S OPEN A WINDOW*");
      break;
    case BAD_COUNT_MODE:
      BTSerial.print("S POLICE WILL COMING SOON*");
      break;
    case BAD_COUNT_AND_AIR_MODE:
      BTSerial.print("S EVERYTHING IS BAD*");
      break;
    case EMERGENCY_MODE:
      BTSerial.print("S GO AWAY*");
      break;
  }
}

ISR(TIMER2_A) {
  if(!doDetecting){
    delayForDetecting++;
    if(delayForDetecting > DELAY_AFTER_DETECTING){
      doDetecting = true;
    }
  }
  if(timeForEmergencyExit != 0){
    timeForEmergencyExit++;
  }
  if(LEDMode == ALARM_LED_MODE){
    blinkingLEDTimer++;
    if(blinkingLEDTimer > BLINKING_PERIOD){
      blinkingNow = !blinkingNow;
    }
  }
  timeCounter++;
}

void buttonInterrupt() {
  Serial.println("Button pressed");
  if(!buttonIsPressed){
    buttonIsPressed = true;
    timeForEmergencyExit = 1;
    Serial.println("Button pressed: true");
  }
}
