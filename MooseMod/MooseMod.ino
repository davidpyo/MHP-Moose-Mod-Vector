/********************************************************************************************************
* MHP Moose Mod Vector
*
* Description
* Program for MHP Moose Mod Vector 
* Forked from the original, removes safety, dart counter functionality. 
* 
* created  13 Jun 2019
* modified 25 Oct 2022
* by TungstenEXE, /u/dpairsoft
* 
* For non commercial use
* 
* If you find my code useful, do support me by subscribing my YouTube Channel, thanks.
*
* Original Creator YouTube Channel Link - Nerf related
* https://www.youtube.com/tungstenexe
* 
* Board used      - Arduino Nano
* Pusher Motor    - 35 mm Generic OOD Solenoid
* FlyWheel Motors - Dual Stage Loki + Kraken Motors, pulsar flywheels
********************************************************************************************************/
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce-Arduino-Wiring
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Bounce2.h>
#include <Servo.h>

#include <SPI.h>
#include <Wire.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Include the PWM library, to change the PWM Frequency for pin controlling the flywheel, found here :
// https://code.google.com/archive/p/arduino-pwm-frequency-library/downloads
// Note: unzip the files to the library folder, you might need to rename the folder name
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <PWM.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Include the Adafruit-GFX library found here :
// https://github.com/adafruit/Adafruit-GFX-Library
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Adafruit_GFX.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Include the Adafruit_SSD1306 library found here :
// https://github.com/adafruit/Adafruit_SSD1306
// you might need to comment away the line "#define SSD1306_128_32" and uncomment the line
// "#define SSD1306_128_64" so that the display works for OLED screen size 128 by 64
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Adafruit_SSD1306.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// PIN Assigment
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// The colors for for my wiring reference only, you can use your own color

#define PIN_FLYWHEEL_MOSFET         3    // (Orange) PIN to control DC Flywheel MOSFET 
#define PIN_OLED_RESET              -1    //          for OLED
#define PIN_REV                     5    // (White)  PIN listening to change in the Nerf Rev Button 
#define PIN_DARTTRIGGER             6    // (Purple) PIN listening to trigger pull event

#define PIN_SOLENOID                9    // (Purple) PIN to control solenoid

#define PIN_BUTTON                  10   // (Grey)   Button for changing menus
#define PIN_SELECTOR                11   // (Green)  Selector switch for Semi/(Burst/Full)

// Note                             A4      (Blue)   are used by the OLED SDA (Blue)
// Note                             A5      (Yellow) are used by the OLED SCL (Yellow)
#define PIN_VOLTREAD                A6   // (Yellow) PIN to receive voltage reading from battery 

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// End Of PIN Assigment
/////////////////////////////////////////////////////////////////////////////////////////////////////////

//#define BURST_UPPER_LIMIT           4    // Maxmimum burst configurable
//#define BURST_LOWER_LIMIT           2    // Minimum burst configurable

#define MODE_SINGLE                 0    // Integer constant to indicate firing single shot
#define MODE_BURST                  1    // Integer constant to indicate firing burst
#define MODE_AUTO                   2    // Integer constant to indicate firing full auto
//#define NUM_OF_MODE                 3    // Number of mode available

//#define DEFAULT_BURSTLIMIT          3    // Default number of burst fire darts

//#define MODE_ROF_LOW                0    // Integer constant to indicate low rate of fire 
//#define MODE_ROF_STANDARD           1    // Integer constant to indicate standard rate of fire 
//#define MODE_ROF_HIGH               2    // Integer constant to indicate highest rate of fire 
//#define NUM_OF_MODE_ROF             3    // Number of ROF available
#define MAIN_MENU                   0    //main menu
#define ROF                         1    //ROF menu
#define AUTO_BURST                  2    //Selection of either burst/auto
#define BURST_LIMIT                 3    //selection of how many darts per burst
#define OP_SCREEN                   4    //operating screen after setup
                                         
#define REV_UP_DELAY                180  // Increase/decrease this to control the flywheel rev-up time (in milliseconds) 

//int     modeROFSelected            = MODE_ROF_HIGH;   // track the ROF selected, set default to High

int     delaySolenoidExtended      = 60; //delay for solenoid to fully extend
int     delaySolenoidRetracted     = 45; //delay from when solenoid to fully retract to allow another extension

int     burstLimit                 = 3;     // darts per burst

int     modeFire                  = MODE_SINGLE; // track the mode of fire, Single, Burst or Auto, Single by default
int     dartToBeFire              = 0;           // track amount of dart(s) to fire when trigger pulled, 0 by default

int     fwLimitArr []             = {75, 100};   // number are in percentage
int     FW_LOW                    = 0;
int     FW_HIGH                   = 1;
int32_t frequency                 = 10000;       //frequency (in Hz) for PWM controlling Flywheel motors
int     fwSpeed                   = 255;
String  speedSelStr               = "";

boolean isRevving                 = false;       // track if blaster firing         
boolean isFiring                  = false;       // track if blaster firing
boolean isBurst                   = false;       // track selector switch behavior for burst/full.        
int     currentScreen             = 0;

boolean exit_menu                 = false;
unsigned long timerSolenoidDetect = 0;
boolean       isSolenoidExtended  = false;

Adafruit_SSD1306 display(PIN_OLED_RESET);

// Declare and Instantiate Bounce objects
Bounce btnRev            = Bounce(); 
Bounce btnTrigger        = Bounce(); 
Bounce switchSelector    = Bounce();
Bounce switchButton      = Bounce();


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: shotFiredHandle
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void shotFiringHandle() {
  if (isFiring) {
    if (isSolenoidExtended) {
      if ((millis() - timerSolenoidDetect) >= delaySolenoidExtended) {
        digitalWrite(PIN_SOLENOID, LOW); // Retract Solenoid
        dartToBeFire--;
        
        isSolenoidExtended = false;
        timerSolenoidDetect = millis();        
      }
    } else { // Solenoid had returned to rest position
      if (dartToBeFire == 0) { //done firing
        isFiring = false;
        if (!isRevving) { // Rev button not pressed
          isRevving = false;
          pwmWrite(PIN_FLYWHEEL_MOSFET, 0);// stop flywheels
        }
      } else if ((millis() - timerSolenoidDetect) >= delaySolenoidRetracted[modeROFSelected]) {
        digitalWrite(PIN_SOLENOID, HIGH); // Extend Solenoid
        isSolenoidExtended = true;
        timerSolenoidDetect = millis();
      }      
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: triggerPressedHandle
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void triggerPressedHandle(int caseModeFire) {  
  //updateSettingDisplay();
    if (!isRevving) {
      pwmWrite(PIN_FLYWHEEL_MOSFET, fwSpeed); // start flywheels
      delay(REV_UP_DELAY);
      isRevving = true;
    }
    
      switch(caseModeFire) {
        case MODE_SINGLE: dartToBeFire++; break;
        case MODE_BURST : dartToBeFire += burstLimit; 
          break;
        case MODE_AUTO  : dartToBeFire += 1000; //generic max value
      }

      // Start Firing    
      if (!isFiring) {
        isFiring = true;
        digitalWrite(PIN_SOLENOID, HIGH); // extend pusher
        timerSolenoidDetect = millis();
        isSolenoidExtended = true;      
          
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: triggerReleasedHandle
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void triggerReleasedHandle() {  
  if (((modeFire == MODE_AUTO) || (modeFire == MODE_BURST)) && isFiring) {
    if (dartToBeFire > 1) {      
      dartToBeFire = 1;    // fire off last shot
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: readVoltage
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void readVoltage() {
  int voltagePinAnalogValue = analogRead(PIN_VOLTREAD);
    
  // you might have to adjust the formula according to the voltage sensor you use
  int   voltagePinValue = (int) (voltagePinAnalogValue / 0.3890 );
  float newVoltage      = (voltagePinValue / 100.0);

  if (!batteryLow) {
    currentVoltage = (newVoltage < currentVoltage) ? newVoltage : currentVoltage;      
  } else {
    currentVoltage = (newVoltage > BATTERY_MIN) ? newVoltage : currentVoltage;  
  }
  batteryLow = (currentVoltage <= BATTERY_MIN);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: updateDisplay
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void updateDisplay() {
  readVoltage();
  int numOfCircle = 1;
  int intCurrentVolt = (int) (currentVoltage * 10);

  if (intCurrentVolt < BATTERY_MIN_3DIGIT) {
    intCurrentVolt = BATTERY_MIN_3DIGIT;
  } else if (intCurrentVolt > BATTERY_MAX_3DIGIT) {
    intCurrentVolt = BATTERY_MAX_3DIGIT;
  }
  
  int batt = map(intCurrentVolt, BATTERY_MIN_3DIGIT, BATTERY_MAX_3DIGIT, 0, 16);

  display.clearDisplay();
  
  display.fillRect(0, 0, (8*batt), 4, WHITE);
  for (int i=1; i<=batt; i++) {
    display.drawLine(i*8, 0, i*8, 4, BLACK);
  }
  
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0,8);
  display.print(">> ");
  display.print(currentVoltage);
  display.println(" volts");  
  
  display.setCursor(0,19);
  display.print(speedSelStr);
  display.println("-TEXE-V6-<T>");  
  
  display.setCursor(0,32);
  
  switch(modeFire) {
      case MODE_SINGLE: 
          display.println("Single Shot");  
        break;
      case MODE_BURST : 
          numOfCircle = burstLimit;
          display.print(burstLimit);          
          display.println(" Rounds Burst");  
        break;
      case MODE_AUTO  : 
          numOfCircle = 10;
          display.println("Full Auto");  
        break;
    }
    
  display.setCursor(0,57);
  display.println(rofLimitStrArr[modeROFSelected]);  
  
  display.setCursor(90,18);
  display.setTextSize(3);
  display.println(dartLeft);  

  for(int i=0; i<numOfCircle; i++) {
    display.fillCircle((i * 9) + 3, 48, 3, WHITE);
  }
  display.display();
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: shutdown
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void shutdownSys() {
  dartToBeFire = 0;
  digitalWrite(PIN_SOLENOID, LOW);
  pwmWrite(PIN_FLYWHEEL_MOSFET, 0);
  isFiring = false;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: main menu
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void menu(){
   display.setTextSize(1);
   display.setColor(WHITE);
   display.setCursor(0,0);
   display.println("MAIN MENU:");
   display.println("Rate of fire");
   
   
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: setup
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() { // initilze  
  //initialize all timers except for 0, to save time keeping functions
  InitTimersSafe();

  //sets the frequency for the specified pin
  bool success = SetPinFrequencySafe(PIN_FLYWHEEL_MOSFET, frequency);
  
  // if the pin frequency was set successfully, turn pin 13 on, a visual check
  // can be commented away in final upload to arduino board
  if(success) {
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);    
  }
  
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  // INPUT PINs setup
  // Note: Most input pins will be using internal pull-up resistor. A fall in signal indicate button pressed.
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
   
  pinMode(PIN_REV,INPUT_PULLUP);              // PULLUP
  btnRev.attach(PIN_REV);
  btnRev.interval(5);
    
  pinMode(PIN_DARTTRIGGER,INPUT_PULLUP);      // PULLUP
  btnTrigger.attach(PIN_DARTTRIGGER);
  btnTrigger.interval(5);

  pinMode(PIN_SELECTOR,INPUT_PULLUP);     // PULLUP
  switchSelector.attach(PIN_SELECTOR);
  switchSelector.interval(5);

  pinMode(PIN_BUTTON,INPUT_PULLUP);     // PULLUP
  switchButton.attach(PIN_BUTTON;
  switchButton.interval(5);

  pinMode(PIN_VOLTREAD, INPUT);               // Not using PULLUP analog read 0 to 1023

  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  // OUTPUT PINs setup
  ///////////////////////////////////////////////////////////////////////////////////////////////////////  

  pinMode (PIN_FLYWHEEL_MOSFET, OUTPUT);
  pwmWrite(PIN_FLYWHEEL_MOSFET, 0);  

  digitalWrite(PIN_SOLENOID, LOW);
  pinMode(PIN_SOLENOID, OUTPUT);
  
  if (digitalRead(PIN_SELECTOR) == LOW){
    modeFire = MODE_AUTO;
  } else {
    modeFire = MODE_SINGLE;
  }       

  
  //fwSpeed     = (digitalRead(PIN_REV) == LOW) ? map(fwLimitArr[FW_HIGH] , 0, 100, 0, 255) : map(fwLimitArr[FW_LOW] , 0, 100, 0, 255);
  //speedSelStr = (digitalRead(PIN_REV) == LOW) ? "H" : "L";
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.clearDisplay();

  if (digitalRead(PIN_DARTTRIGGER) == LOW){
    while(1){
          btnTrigger.update(); //trigger acts as "select"
          switchButton.update(); //button acts as "enter"
         
          switch(newState){
            case (MAIN_MENU)):
            if (newState != currentState){
              
            }
            
            //code
            break;
            case (ROF)):
            //code
            break;
            case (AUTO_BURST)):
            //code
            break;
            case (BURST_LIMIT)):
            //code
            break;

          }





          
          if(switchButton.changed() || btnTrigger.changed()){
            if (btnTrigger.read() == LOW && switchButton.read() == LOW){
              break; //exits infinite loop
            }
          }  
    }
  }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function: loop
//           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() { // Main Loop  
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // Update all buttons
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    
    btnRev.update();
    btnTrigger.update();
    switchSelector.update();
    switchButton.update();    

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Listen to Rev Press/Release
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    if (btnRev.fell()) {                   // press    
        isRevving = true;
        // digitalWrite(PIN_FLYWHEEL_MOSFET, HIGH); // start flywheels
        pwmWrite(PIN_FLYWHEEL_MOSFET, fwSpeed);        
    } else if (btnRev.rose()) {        // released
      isRevving = false;
      if (!isFiring) {        
        // digitalWrite(PIN_FLYWHEEL_MOSFET, LOW); // stop flywheels
        pwmWrite(PIN_FLYWHEEL_MOSFET, 0);
      }
    }
  
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Listen to Trigger Pull/Release
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    if (btnTrigger.fell()) {               // pull
        triggerPressedHandle(modeFire);        
    } else if (btnTrigger.rose()) {        // released
        triggerReleasedHandle();
    }
  
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Listen to Firing
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    shotFiringHandle();
  
    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // Listen to Firing Mode change: Single Shot, Burst, Full Auto
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    if (switchSelector.changed()) {  
        if (switchSelector.read()){ //if selector is in full/burst position
          if(isBurst){
            modeFire = MODE_BURST;
          } else {
            modeFire = MODE_AUTO;
          }
        } else { // single
           modeFire = MODE_SINGLE;
        }
    }    

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// END OF PROGRAM           
/////////////////////////////////////////////////////////////////////////////////////////////////////////
