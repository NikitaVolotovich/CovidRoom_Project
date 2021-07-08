#include <SimpleTimer.h>
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
#define EMERGENCY_BUTTON 6
#define MOTOR_PIN 8

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

#define MAXIMUM_AIR_QUALITY 400
#define MAXIMUM_PEOPLE 10
#define MAXIMUM_LED_BRIGHT 128

//Anything else ?

int mode = 0;
int airQuality = 0;
float ultrasonicOutside = 0, ultrasonicInside = 0;
int ultrasonicSystematicError = 5;
int ultrasonicOutsideNormalDistance = 50, ultrasonicInsideNormalDistance = 60;
bool buttonIsPressed = false, doorIsOpen = false;
float distance, sensity;
int timeCounter = 0;
int peopleCounter = 0;
bool isOutsideUltrasonicTriggered = false, isInsideUltrasonicTriggered = false;
//LCD//
int g,b;
int t=10;
int r=255;

//------------------------
SimpleTimer timer;
Adafruit_NeoPixel outsideLED = Adafruit_NeoPixel(1, OUTSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel insideLED = Adafruit_NeoPixel(1, INSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Servo motor;
//Adafruit_NeoPixel RGB_Strip = Adafruit_NeoPixel(numberOfLEDs, LEDPin, NEO_GRB + NEO_KHZ800);
DFRobot_RGBLCD lcd(16,2);

void setup() {
  lcd.init();

  motor.attach(MOTOR_PIN);
  motor.writeMicroseconds(800); 
  
  timer.setInterval(1000, every100msTimer);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_ECHO, INPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_TRIG, OUTPUT); 
  pinMode(ULTRASONIC_SENSOR_INSIDE_ECHO, INPUT); 
  pinMode(EMERGENCY_BUTTON, INPUT);
  
  outsideLED.begin();
//  outsideLED.show();
  insideLED.begin();
//  insideLED.show();

  Serial.begin(115200);

  outsideLED.clear();
  outsideLED.setPixelColor(0, outsideLED.Color(60, 60, 60));
  outsideLED.show();
  
  insideLED.clear();   
  insideLED.setPixelColor(0, insideLED.Color(125, 0, 0));
  insideLED.show();
}

void loop() {
  updateSensors();
  modeSelector();
  showOnLCD();
  outputEverything();
  controlInsideLED();
  controlOutsideLED();


}

void every100msTimer(void) {         // Check is ready a first timer
    timeCounter++;
}

//----------------------------------
bool alertAirQuality = false;
void controlInsideLED() {
  if(airQuality != 0 && airQuality < MAXIMUM_AIR_QUALITY) {
    float indexAir = (float) MAXIMUM_AIR_QUALITY/airQuality;
    float redColorBright_In = MAXIMUM_LED_BRIGHT/indexAir, greenColorBright_In = MAXIMUM_LED_BRIGHT - redColorBright_In;
    insideLED.clear();   
    insideLED.setPixelColor(0, insideLED.Color(((int) redColorBright_In), ((int) greenColorBright_In), 0));
    insideLED.show();
  }
}
void controlOutsideLED()
{
  if(peopleCounter == 0)
  {
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
  if(airQuality < MAXIMUM_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
    mode = GOOD_MODE;
  } else if (airQuality > MAXIMUM_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed){
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

//      colorWipe(insideLED.Color(126, 126, 126), &insideLED);
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
   Serial.println("___________________________");
  if(ultrasonicOutsideNormalDistance - ultrasonicSystematicError > ultrasonicOutside){
    if( isInsideUltrasonicTriggered==true){ 
      Serial.println("1: Runs-outside to inside");
      peopleCounter--;
      isOutsideUltrasonicTriggered=false;
      isInsideUltrasonicTriggered=false; //s2
      return;
    } else {  
      Serial.println("2: Runs-outside exec first");
      isOutsideUltrasonicTriggered=true;
    }
    
  }
  
  if(ultrasonicInsideNormalDistance - ultrasonicSystematicError>ultrasonicInside){  //from inside to outside 
    if(isOutsideUltrasonicTriggered==true){
      Serial.println("3: Runs-inside to outside");
      peopleCounter++;
      isOutsideUltrasonicTriggered=false;
      isInsideUltrasonicTriggered=false;
      return;
    } else {
      Serial.println("4: Runs-inside exec first");

      isInsideUltrasonicTriggered = true;
    }   

  }
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
  lcd.print("peopleCounter: ");
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

void testLED(Adafruit_NeoPixel anyLED){
  anyLED.clear();
  anyLED.setBrightness(128);
  for(int i = 0; i < 255; i++){
    for(int j = 0; i < 255; i++){
         anyLED.clear();
         anyLED.setPixelColor(0, anyLED.Color(0, j, i));
         anyLED.show();
      
    }
  }
}
