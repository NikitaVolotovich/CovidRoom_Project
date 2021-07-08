#include <GyverTimers.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#include <Wire.h>
#include "DFRobot_RGBLCD.h"
#include <Servo.h>

#define OUTSIDE_LED_PIN 2
#define INSIDE_LED_PIN 3
#define BLUETOOTH_RX_PIN 4
#define BLUETOOTH_TX_PIN 5
#define EMERGENCY_BUTTON 8
#define MOTOR_PIN 6

#define ULTRASONIC_SENSOR_OUTSIDE_TRIG 9
#define ULTRASONIC_SENSOR_OUTSIDE_ECHO 10
#define ULTRASONIC_SENSOR_INSIDE_TRIG 11
#define ULTRASONIC_SENSOR_INSIDE_ECHO 12

#define AQ_SENSOR A2

#define START_MODE 0
#define NORMAL_MODE 1
#define BAD_AIR_MODE 2
#define BAD_COUNT_MODE 3
#define BAD_COUNT_AND_AIR_MODE 4
#define EMERGENCY_MODE 5

#define NORMAL_LED_MODE 0
#define ALARM_LED_MODE 1

#define NORMAL_AIR_QUALITY 300
#define MAXIMUM_AIR_QUALITY 600
#define MAXIMUM_PEOPLE 10
#define MAXIMUM_LED_BRIGHT 128
#define SENSOR_MISTAKE_PERCENT 0.05

#define DOOR_OPEN_STATE 110
#define DOOR_CLOSE_STATE 700
#define TIME_FOR_EXIT 1000

int mode = 0, LEDMode = 0;
int airQuality = 0;

int delayForDetecting = 201;
bool doDetecting = true;
float ultrasonicOutside = 0, ultrasonicInside = 0;
int ultrasonicSystematicErrorOutside, ultrasonicSystematicErrorInside;
int ultrasonicOutsideNormalDistance  = 0, ultrasonicInsideNormalDistance = 0;
int peopleCounter = 0;

bool buttonIsPressed = false, doorIsOpen = false;
float distance, sensity;
int timeCounter = 0, timeForEmergencyExit = 0;

bool somethingWasDetected = false;

int LCDRed,LCDGreen, LCDBlue;
int LCDTime = 0;
///////

Adafruit_NeoPixel outsideLED = Adafruit_NeoPixel(1, OUTSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel insideLED = Adafruit_NeoPixel(1, INSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Servo motor;
DFRobot_RGBLCD lcd(16,2);

void setup() {
  lcd.init();

  motor.attach(MOTOR_PIN);
  motor.writeMicroseconds(800); 
  
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_ECHO, INPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_TRIG, OUTPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_ECHO, INPUT); 
  pinMode(EMERGENCY_BUTTON, INPUT);
  attachInterrupt(1, buttonInterrupt, FALLING);
  
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
}

void loop() {
  ultrasonicValueUpdate();
  detectPerson();
  updatePeripherals();
  modeSelector();
  modeExecutor();
//  outputEverything();
}

void controlInsideLED() {
  if(airQuality != 0 && airQuality < MAXIMUM_AIR_QUALITY) {
    float indexAir = (float) MAXIMUM_AIR_QUALITY / airQuality;
    float redColorBright_In = MAXIMUM_LED_BRIGHT / indexAir, greenColorBright_In = MAXIMUM_LED_BRIGHT - redColorBright_In;
    insideLED.clear();   
    insideLED.setPixelColor(0, insideLED.Color(((int) redColorBright_In), ((int) greenColorBright_In), 0));
    insideLED.show();
  }
}

void controlOutsideLED() {
  if(peopleCounter == 0) {
    outsideLED.clear();  
    outsideLED.setPixelColor(0, outsideLED.Color(0, MAXIMUM_LED_BRIGHT, 0));
    outsideLED.show();
  } else if(peopleCounter != 0 && peopleCounter <= MAXIMUM_PEOPLE) {
    float  indexPeople = (float) MAXIMUM_PEOPLE / peopleCounter;
    int redColorBright_Out = MAXIMUM_LED_BRIGHT / indexPeople;
    int greenColorBright_Out = MAXIMUM_LED_BRIGHT - redColorBright_Out;
    outsideLED.clear();  
    outsideLED.setPixelColor(0, outsideLED.Color(((int) redColorBright_Out), ((int) greenColorBright_Out), 0));
    outsideLED.show();
  } 
}


void modeSelector() {
  if(airQuality < NORMAL_AIR_QUALITY && peopleCounter == 0 && !buttonIsPressed){
    mode = START_MODE;
  } else if(airQuality < NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && peopleCounter != 0 && !buttonIsPressed){
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
    case START_MODE:
    
      openDoor();
      break;
    case NORMAL_MODE:
    
      openDoor();
      break;
    case BAD_AIR_MODE:
    
      closeDoor();
      break;
    case BAD_COUNT_MODE:

      closeDoor();
      break;
    case BAD_COUNT_AND_AIR_MODE:
    
      break;
    case EMERGENCY_MODE:
    
      openDoor();
      break;
  } 
}



void updatePeripherals() {
  showOnLCD();
  controlInsideLED();
  controlOutsideLED();
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
      peopleCounter--;
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
  if(LEDMode == NORMAL_LED_MODE){
    LCDRed = (abs(sin(3.14*LCDTime/180)))*255;       
    LCDGreen = (abs(sin(3.14*(LCDTime+60)/180)))*255;
    LCDBlue = (abs(sin(3.14*(LCDTime+120)/180)))*255;
    LCDTime += 3;
  } else if(LEDMode == ALARM_LED_MODE){
    LCDRed = 255;      
    LCDGreen = 0;
    LCDBlue = 0;
  }
//  lcd.clear();
  lcd.setRGB(LCDRed, LCDGreen, LCDBlue);    
  lcd.setCursor(0,0);
  lcd.print("Counter: ");
  lcd.print(peopleCounter);
  lcd.setCursor(0,1);
  lcd.print("AirQuality: ");
  lcd.print(airQuality);
}

void outputEverything() {
  Serial.print("Current mode: ");
  Serial.println(mode);
  Serial.print("Ultrasonic outside cm: ");
  Serial.println(ultrasonicOutside);
  Serial.print("Ultrasonic inside cm: ");
  Serial.println(ultrasonicInside);
  Serial.print("Button is pressed: ");
  Serial.println(buttonIsPressed);
  Serial.print("Air quality: ");
  Serial.println(airQuality);
  Serial.print("People counter: ");
  Serial.println(peopleCounter);
  Serial.print("Time counter: ");
  Serial.println(timeCounter);
}

ISR(TIMER2_A) {

  if(!doDetecting){
    delayForDetecting++;
    if(delayForDetecting > 200){
      doDetecting = true;
    }
  }
  if(timeForEmergencyExit != 0){
    timeForEmergencyExit++;
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
