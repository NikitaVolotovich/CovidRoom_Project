#ifdef __AVR__
#include <avr/power.h>
#endif
#include <GyverTimers.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "DFRobot_RGBLCD.h"
#include <Servo.h>
#include <SoftwareSerial.h>

// PINOUT
#define EMERGENCY_BUTTON 2
#define INSIDE_LED_PIN 3
#define BLUETOOTH_RX_PIN 4
#define BLUETOOTH_TX_PIN 5
#define MOTOR_PIN 6
#define OUTSIDE_LED_PIN 8
#define ULTRASONIC_SENSOR_OUTSIDE_TRIG 9
#define ULTRASONIC_SENSOR_OUTSIDE_ECHO 10
#define ULTRASONIC_SENSOR_INSIDE_TRIG 11
#define ULTRASONIC_SENSOR_INSIDE_ECHO 12
#define SOUND_PIN 13
#define AQ_SENSOR A2

// MODES
#define NORMAL_MODE 0
#define BAD_AIR_MODE 1
#define BAD_COUNT_MODE 2
#define BAD_COUNT_AND_AIR_MODE 3
#define EMERGENCY_MODE 4
#define NORMAL_LED_MODE 0
#define ALARM_LED_MODE 1

// DEFINES CONDITION VALUES
#define NORMAL_AIR_QUALITY 300
#define MAXIMUM_AIR_QUALITY 450
#define MAXIMUM_PEOPLE 10
#define MAXIMUM_LED_BRIGHT 128
#define SENSOR_MISTAKE_PERCENT 0.3
#define DOOR_OPEN_STATE 100
#define DOOR_CLOSE_STATE 0

// DEFINES CONDITION PERIODS
#define DELAY_AFTER_DETECTING 500
#define BLINKING_PERIOD 10
#define TIMER_PERIOD 1000
#define TIMER_FREQUENCY 100
#define TIME_FOR_EXIT 30000

// NOTES
#define NOTE_C4  262
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_B3  247

// MODES
int mode = 0, LEDMode = 0;
int airQuality = 0;

// COUNTERS AND DISTANCE
int ultrasonicOutside = 0, ultrasonicInside = 0;
int ultrasonicSystematicErrorOutside = 0, ultrasonicSystematicErrorInside = 0;
int ultrasonicOutsideNormalDistance  = 0, ultrasonicInsideNormalDistance = 0;
int peopleCounter = 0, previousPeopleCounter = 0;
float distance = 0, sensity = 0;

// TIME COUNTERS
int delayForDetecting = 0, timeForEmergencyExit = 0, blinkingLEDDelay = 0;
int LCDTime = 0;

// BOOLEANS
bool doDetecting = true, somethingWasDetected = false, blinkingNow = false;
bool buttonIsPressed = false, doorIsOpen = false, isPlayMusic = false;

// COLORS
int peopleCounterLEDRed = 0, peopleCounterLEDGreen = 0, peopleCounterLEDBlue = 0;
int airQualityLEDRed = 0, airQualityLEDGreen = 0, airQualityLEDBlue = 0;
int modeIndicatorRed = 0, modeIndicatorGreen = 0, modeIndicatorBlue = 0;
int LCDRed = 0, LCDGreen = 0, LCDBlue = 0;

// MUSIC
int melody[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
int noteDurations[] = {4, 8, 8, 4, 4, 4, 4, 4};
int thisNote = 0;

// OBJECTS
SoftwareSerial BTSerial(BLUETOOTH_RX_PIN, BLUETOOTH_TX_PIN);
Adafruit_NeoPixel outsideLED = Adafruit_NeoPixel(1, OUTSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel insideLED = Adafruit_NeoPixel(1, INSIDE_LED_PIN, NEO_GRB + NEO_KHZ800);
Servo motor;
DFRobot_RGBLCD lcd(16, 2);

void setup() {
  lcd.init();
  Serial.begin(115200);
  BTSerial.begin(115200);
  motor.attach(MOTOR_PIN);
  motor.write(0);

  pinMode(ULTRASONIC_SENSOR_OUTSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_OUTSIDE_ECHO, INPUT);
  pinMode(ULTRASONIC_SENSOR_INSIDE_TRIG, OUTPUT);
  pinMode(ULTRASONIC_SENSOR_INSIDE_ECHO, INPUT);
  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);

  while (ultrasonicOutsideNormalDistance == 0 && ultrasonicInsideNormalDistance == 0 ) {
    ultrasonicValueUpdate();
    ultrasonicOutsideNormalDistance = ultrasonicOutside;
    ultrasonicInsideNormalDistance  = ultrasonicInside;

  }

  ultrasonicSystematicErrorOutside = ultrasonicOutsideNormalDistance * SENSOR_MISTAKE_PERCENT;
  ultrasonicSystematicErrorInside = ultrasonicInsideNormalDistance * SENSOR_MISTAKE_PERCENT;

  outsideLED.begin();
  insideLED.begin();

  attachInterrupt(digitalPinToInterrupt(EMERGENCY_BUTTON), buttonInterrupt, FALLING);
  Timer2.setPeriod(TIMER_PERIOD);
  Timer2.setFrequency(TIMER_FREQUENCY);
  Timer2.enableISR(CHANNEL_B);
}

void loop() {
  updatePeripherals();
  detectPerson();
  modeExecutor(mode = modeSelector());
  Serial.println(String(ultrasonicOutside) + "\t" + String(ultrasonicInside));
}

void controlLED(Adafruit_NeoPixel &anyLED, int red, int green, int blue) {
  anyLED.clear();
  anyLED.setPixelColor(0, anyLED.Color(red, green, blue));
  anyLED.show();
}


int modeSelector() {
  if (airQuality < NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed) {
    return NORMAL_MODE;
  } else if (airQuality > NORMAL_AIR_QUALITY && peopleCounter <= MAXIMUM_PEOPLE && !buttonIsPressed) {
    return BAD_AIR_MODE;
  } else if (airQuality < NORMAL_AIR_QUALITY && peopleCounter > MAXIMUM_PEOPLE && !buttonIsPressed) {
    return BAD_COUNT_MODE;
  } else if (airQuality > NORMAL_AIR_QUALITY && peopleCounter > MAXIMUM_PEOPLE && !buttonIsPressed) {
    return BAD_COUNT_AND_AIR_MODE;
  } else if (buttonIsPressed) {
    return EMERGENCY_MODE;
  }
}

void modeExecutor(int currentMode) {
  switch (currentMode) {
    case NORMAL_MODE:
      LEDMode = NORMAL_LED_MODE;
      thisNote = 0;
      openDoor();
      break;

    case BAD_AIR_MODE:
      LEDMode = ALARM_LED_MODE;
      closeDoor();
      playMusic();
      break;

    case BAD_COUNT_MODE:
      LEDMode = ALARM_LED_MODE;
      closeDoor();
      playMusic();
      break;

    case BAD_COUNT_AND_AIR_MODE:
      LEDMode = ALARM_LED_MODE;
      closeDoor();
      playMusic();
      break;

    case EMERGENCY_MODE:
      LEDMode = ALARM_LED_MODE;
      openDoor();
      playMusic();
      break;
  }
}

void updatePeripherals() {
  airQuality = analogRead(AQ_SENSOR);
  ultrasonicValueUpdate();
  colorRegulator(LEDMode);
  controlLED(outsideLED, peopleCounterLEDRed, peopleCounterLEDGreen, peopleCounterLEDBlue);
  controlLED(insideLED, airQualityLEDRed, airQualityLEDGreen, airQualityLEDBlue);
  sendDataToDashboard();
  showOnLCD("People: ", peopleCounter, "Air: ", airQuality);

}

void ultrasonicValueUpdate() {
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_SENSOR_OUTSIDE_TRIG, LOW);

  long duration = pulseIn(ULTRASONIC_SENSOR_OUTSIDE_ECHO, HIGH);
  int temp = duration * 0.034 / 2;
  if(temp < 1000){
    ultrasonicOutside = temp;
  }
  

  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_SENSOR_INSIDE_TRIG, LOW);

  duration = pulseIn(ULTRASONIC_SENSOR_INSIDE_ECHO, HIGH);
  temp = duration * 0.034 / 2;
  if(temp < 1000){
    ultrasonicInside = temp;
  }
}

void detectPerson() {
  if (doDetecting) {
    if (ultrasonicOutside < ultrasonicOutsideNormalDistance - ultrasonicSystematicErrorOutside && somethingWasDetected == false) {
      peopleCounter++;
      somethingWasDetected = true;
    }
    if (ultrasonicInside < ultrasonicInsideNormalDistance - ultrasonicSystematicErrorInside && somethingWasDetected == false) {
      if (peopleCounter != 0) {
        peopleCounter--;
      }
      somethingWasDetected = true;
    }
    if (ultrasonicInside + ultrasonicSystematicErrorInside > ultrasonicInsideNormalDistance &&
        ultrasonicOutside + ultrasonicSystematicErrorOutside > ultrasonicOutsideNormalDistance &&
        ultrasonicInside < ultrasonicInsideNormalDistance + ultrasonicSystematicErrorInside &&
        ultrasonicOutside < ultrasonicOutsideNormalDistance + ultrasonicSystematicErrorOutside &&
        somethingWasDetected == true) {
      doDetecting = false;
      somethingWasDetected = false;
    }
  }
}

void openDoor() {
  if (doorIsOpen == false) {
    motor.write(DOOR_OPEN_STATE);
    doorIsOpen = true;
  }
}

void closeDoor() {
  if (doorIsOpen == true) {
    motor.write(DOOR_CLOSE_STATE);
    doorIsOpen = false;
  }
}

void showOnLCD(String firstLineString, int firstLineValue, String secondLineString, int secondLineValue) {
  if (peopleCounter != previousPeopleCounter) {
    lcd.clear();
  }
  previousPeopleCounter = peopleCounter;
  lcd.setRGB(LCDRed, LCDGreen, LCDBlue);
  lcd.setCursor(0, 0);

  lcd.print(firstLineString + String(firstLineValue));
  lcd.setCursor(0, 1);
  lcd.print(secondLineString + String(secondLineValue));
}

void colorRegulator(int currentMode) {
  switch (currentMode) {
    case NORMAL_LED_MODE:
      airQualityLEDRed = MAXIMUM_LED_BRIGHT / ((float) MAXIMUM_AIR_QUALITY / airQuality);
      airQualityLEDGreen = MAXIMUM_LED_BRIGHT - airQualityLEDRed;
      if (peopleCounter == 0) {
        peopleCounterLEDRed = 0;
        peopleCounterLEDGreen = MAXIMUM_LED_BRIGHT;
      } else {
        peopleCounterLEDRed = MAXIMUM_LED_BRIGHT / ((float) MAXIMUM_PEOPLE / peopleCounter);
        peopleCounterLEDGreen = MAXIMUM_LED_BRIGHT - peopleCounterLEDRed;
      }
      LCDRed = (abs(sin(3.14 * LCDTime / 180))) * 255;
      LCDGreen = (abs(sin(3.14 * (LCDTime + 60) / 180))) * 255;
      LCDBlue = (abs(sin(3.14 * (LCDTime + 120) / 180))) * 255;
      LCDTime += 3;
      modeIndicatorRed = 0;
      modeIndicatorGreen = MAXIMUM_LED_BRIGHT;
      break;
    case ALARM_LED_MODE:
      if (blinkingNow) {
        airQualityLEDRed = MAXIMUM_LED_BRIGHT;
        airQualityLEDGreen = 0;
        peopleCounterLEDRed = MAXIMUM_LED_BRIGHT;
        peopleCounterLEDGreen = 0;
        LCDRed = 255;
        LCDGreen = 0;
        LCDBlue = 0;
        if (mode == BAD_AIR_MODE) {
          modeIndicatorRed = 255;
          modeIndicatorGreen = 100;
        } else {
          modeIndicatorRed = MAXIMUM_LED_BRIGHT;
          modeIndicatorGreen = 0;
        }
      } else if (!blinkingNow) {
        modeIndicatorRed = 0;
        modeIndicatorGreen = 0;
        airQualityLEDRed = 0;
        airQualityLEDGreen = 0;
        peopleCounterLEDRed = 0;
        peopleCounterLEDGreen = 0;
        LCDRed = 0;
        LCDGreen = 0;
        LCDBlue = 0;
      }
      break;
  }
}

void playMusic() {
  if (thisNote < 8 && LEDMode == ALARM_LED_MODE) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(SOUND_PIN, melody[thisNote], noteDuration);
    int pauseBetweenNotes = noteDuration ;
    delay(pauseBetweenNotes);
    noTone(SOUND_PIN);
    thisNote++;
  }
}

void sendDataToDashboard() {
  BTSerial.print("*M" + String(mode));
  BTSerial.print("*O" + String(ultrasonicOutside));
  BTSerial.print("*I" + String(ultrasonicInside));
  BTSerial.print("*B" + String(buttonIsPressed));
  BTSerial.print("*A" + String(airQuality));
  BTSerial.print("*L" + String(doDetecting));
  BTSerial.print("*P" + String(peopleCounter));
  BTSerial.print("*T" + String(delayForDetecting));
  BTSerial.print("*CR" + String(modeIndicatorRed) + "G" + String(modeIndicatorGreen) + "B" + String(modeIndicatorBlue));
  BTSerial.print("*QR" + String(airQualityLEDRed) + "G" + String(airQualityLEDGreen * 2) + "B" + String(airQualityLEDBlue));
  BTSerial.print("*KR" + String(peopleCounterLEDRed) + "G" + String(peopleCounterLEDGreen) + "B" + String(peopleCounterLEDBlue));
  BTSerial.print("*W" + String(ultrasonicOutsideNormalDistance));
  BTSerial.print("*E" + String(ultrasonicInsideNormalDistance));
  BTSerial.print("*Y" + String(ultrasonicSystematicErrorOutside));
  BTSerial.print("*Z" + String(ultrasonicSystematicErrorInside));

  if ( LEDMode == ALARM_LED_MODE) {
    BTSerial.print("*R QUARANTINE SUPERMARKET");
  } else {
    BTSerial.print("*R REGULAR SUPERMARKET");
  }

  switch (mode) {
    case NORMAL_MODE:
      BTSerial.print("*S JUST CHILL");
      BTSerial.print("*D OPENED");
      break;
    case BAD_AIR_MODE:
      BTSerial.print("*S OPEN A WINDOW");
      BTSerial.print("*D CLOSED");
      break;
    case BAD_COUNT_MODE:
      BTSerial.print("*S POLICE WILL COMING SOON");
      BTSerial.print("*D CLOSED");
      break;
    case BAD_COUNT_AND_AIR_MODE:
      BTSerial.print("*S EVERYTHING IS BAD");
      BTSerial.print("*D CLOSED");
      break;
    case EMERGENCY_MODE:
      BTSerial.print("*S GO AWAY");
      BTSerial.print("*D OPENED*");
      break;
  }
}

//***  INTERRUPTS  ***//

ISR(TIMER2_B) {
  if (!doDetecting) { // DELAY AFTER DETECTING
    delayForDetecting++;
    if (delayForDetecting > DELAY_AFTER_DETECTING) {
      delayForDetecting = 0;
      doDetecting = true;
    }
  }
  if (timeForEmergencyExit != 0) { // DELAY WORKING EMERGENCY MODE
    timeForEmergencyExit++;
    if (timeForEmergencyExit > TIME_FOR_EXIT) {
      timeForEmergencyExit = 0;
      buttonIsPressed = false;
    }
  }
  if (LEDMode == ALARM_LED_MODE) { // DELAY FOR LED BLINKING
    blinkingLEDDelay++;
    if (blinkingLEDDelay > BLINKING_PERIOD) {
      blinkingLEDDelay = 0;
      blinkingNow = !blinkingNow;
    }
  }
}

void buttonInterrupt() {
  if (!buttonIsPressed) {
    buttonIsPressed = true;
    timeForEmergencyExit = 1;
  }
}
