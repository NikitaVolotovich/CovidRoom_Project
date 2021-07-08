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
#define GOOD_MODE 1
#define BAD_AIR_MODE 2
#define BAD_COUNT_MODE 3
#define EMERGENCY_MODE 4

#define NORMAL_AIR_QUALITY 300
#define MAXIMUM_AIR_QUALITY 600
#define MAXIMUM_PEOPLE 10
#define MAXIMUM_LED_BRIGHT 128

int mode = 0;
int airQuality = 0;

int delayForDetecting = 201;
bool doDetecting = true;
float ultrasonicOutside = 0, ultrasonicInside = 0;
int ultrasonicSystematicError = 20;
int ultrasonicOutsideNormalDistance = 180, ultrasonicInsideNormalDistance = 180;
int peopleCounter = 0;

bool buttonIsPressed = false, doorIsOpen = false;
float distance, sensity;
int timeCounter = 0;

bool somethingWasDetected = false;

//LCD//
int r,g,b;
int t=0;
///////

Adafruit_NeoPixel outsideLED = Adafruit_NeoPixel(1, OUTSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel insideLED = Adafruit_NeoPixel(1, INSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Servo motor;
DFRobot_RGBLCD lcd(16,2);

void setup() {
  
  Timer2.setPeriod(100000);
  Timer2.enableISR(CHANNEL_A);
  
  lcd.init();

  motor.attach(MOTOR_PIN);
  motor.writeMicroseconds(800); 
  
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_ECHO, INPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_TRIG, OUTPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_ECHO, INPUT); 
  pinMode(EMERGENCY_BUTTON, INPUT);
  
  outsideLED.begin();
  insideLED.begin();

  Serial.begin(115200);
}

void loop() {
  updateSensors();
  modeSelector();
  showOnLCD();
  outputEverything();

//  openDoor();
//  delay(2000);
//  closeDoor();
//  delay(2000);
}

void controlInsideLED() {
  if(airQuality != 0 && airQuality < MAXIMUM_AIR_QUALITY) {
    float indexAir = (float) MAXIMUM_AIR_QUALITY/airQuality;
    float redColorBright_In = MAXIMUM_LED_BRIGHT/indexAir, greenColorBright_In = MAXIMUM_LED_BRIGHT - redColorBright_In;
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
    Serial.println("WE ARE HERE");
    float  indexPeople = (float) MAXIMUM_PEOPLE/peopleCounter;
    int redColorBright_Out = MAXIMUM_LED_BRIGHT/indexPeople;
    int greenColorBright_Out = MAXIMUM_LED_BRIGHT - redColorBright_Out;
    Serial.println(redColorBright_Out);
    Serial.println(greenColorBright_Out);
    outsideLED.clear();  
    outsideLED.setPixelColor(0, outsideLED.Color(((int) redColorBright_Out), ((int) greenColorBright_Out), 0));
    outsideLED.show();
  } 
}


void modeSelector() {
  if(airQuality < NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = GOOD_MODE;
  } else if (airQuality > NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = BAD_AIR_MODE;
  } else if (peopleCounter > MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = BAD_COUNT_MODE;
  } else if (buttonIsPressed){
    mode = EMERGENCY_MODE;
  }
}


void modeExecutor() {
  switch (mode) {
    case START_MODE:
    
      openDoor();
      break;
    case GOOD_MODE:
    
      openDoor();
      break;
    case BAD_AIR_MODE:
    
      closeDoor();
      break;
    case BAD_COUNT_MODE:

      closeDoor();
      break;
    case EMERGENCY_MODE:
    
      openDoor();
      break;
  }
}



void updateSensors() {
  ultrasonicValueUpdate();
  detectPerson();
  airQuality = analogRead(AQ_SENSOR);

  if (digitalRead(EMERGENCY_BUTTON) == HIGH) {
    buttonIsPressed = true;
  } else {
    buttonIsPressed = false;
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

  long duration2 = pulseIn(ULTRASONIC_SENSOR_INSIDE_ECHO, HIGH);
  ultrasonicInside = duration2 * 0.034 / 2;
  
}

void detectPerson() {
  if(delayForDetecting > 200){
    if(ultrasonicOutside < ultrasonicOutsideNormalDistance - ultrasonicSystematicError && somethingWasDetected == false){
      Serial.println("OUTSIDE++");
      peopleCounter++;
      somethingWasDetected = true;
    }
    if(ultrasonicInside < ultrasonicInsideNormalDistance - ultrasonicSystematicError && somethingWasDetected == false){
      Serial.println("INSIDE--");
      peopleCounter--;
      somethingWasDetected = true;
    }
    if(ultrasonicInside + ultrasonicSystematicError > ultrasonicInsideNormalDistance &&
    ultrasonicOutside + ultrasonicSystematicError > ultrasonicOutsideNormalDistance &&
    ultrasonicInside < ultrasonicInsideNormalDistance + ultrasonicSystematicError &&
    ultrasonicOutside < ultrasonicOutsideNormalDistance + ultrasonicSystematicError &&
    somethingWasDetected == true
    ){
      Serial.print("BECOMES FALSE! INSIDE: ");
      Serial.print(ultrasonicInside);
      Serial.print("\t OUTSIDE: ");
      Serial.println(ultrasonicOutside);
      doDetecting = false;
      delayForDetecting = 0;
      somethingWasDetected = false;
    }
  }
//  Serial.println("==========");
}

void openDoor() {
  if(doorIsOpen == false){
    motor.write(110);
    doorIsOpen = true;
  }
}

void closeDoor() {
  if(doorIsOpen == true){
    motor.write(700);
    doorIsOpen = false;
  }
}

void showOnLCD(){
  r= (abs(sin(3.14*t/180)))*255;       
  g= (abs(sin(3.14*(t+60)/180)))*255;
  b= (abs(sin(3.14*(t+120)/180)))*255;
  t=0;
  lcd.setRGB(r, g, b);    
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
  detectPerson();
  if(!doDetecting){
    delayForDetecting++;
  }
  timeCounter++;
}
