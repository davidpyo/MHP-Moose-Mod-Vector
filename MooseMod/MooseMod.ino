// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce-Arduino-Wiring

#include <Bounce2.h>
#include <Servo.h>

#include <SPI.h>
#include <Wire.h>

// Include the PWM library, to change the PWM Frequency for pin controlling the flywheel, found here :
// https://code.google.com/archive/p/arduino-pwm-frequency-library/downloads

#include <PWM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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



#define MODE_SINGLE                 0    // Integer constant to indicate firing single shot
#define MODE_BURST                  1    // Integer constant to indicate firing burst
#define MODE_AUTO                   2    // Integer constant to indicate firing full auto

#define MAIN_MENU                   0    //main menu
#define ROF                         1    //ROF menu
#define AUTO_BURST                  2    //Selection of either burst/auto
#define BURST_LIMIT                 3    //selection of how many darts per burst
#define PWM                         4
<<<<<<< HEAD
#define MAXSOLENOIDDELAY            100  //controls how slow of a ROF you can set
#define MINSOLENOIDDELAY            55                                       
#define REV_UP_DELAY                180  // Increase/decrease this to control the flywheel rev-up time (in milliseconds) 

int     delaySolenoidExtended      = 40; //delay for solenoid to fully extend

//#define MAXSOLENOIDDELAY            100  //controls how slow of a ROF you can set
#define MINSOLENOIDDELAY            45                                       
#define REV_UP_DELAY                40   // Increase/decrease this to control the flywheel rev-up time (in milliseconds) 
//Adafruit_SSD1306 display(PIN_OLED_RESET);
#define SCREEN_WIDTH                128 // OLED display width, in pixels
#define SCREEN_HEIGHT               64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET                 -1

int     delayOffset                = 0;
int     delaySolenoidExtended      = 60; //delay for solenoid to fully extend

int     delaySolenoidRetracted     = MINSOLENOIDDELAY; //delay from when solenoid to fully retract to allow another extension
int     burstLimit                 = 3;     // darts per burst
int     modeFire                  = MODE_SINGLE; // track the mode of fire, Single, Burst or Auto, Single by default
int     dartToBeFire              = 0;           // track amount of dart(s) to fire when trigger pulled, 0 by default

int32_t frequency                 = 10000;       //frequency (in Hz) for PWM controlling Flywheel motors
int     fwSpeed                   = 255;         //flywheel speed from 0-255
int     PWMSetting                = 100;         //in percentage
boolean isRevving                 = false;       // track if blaster firing         
boolean isFiring                  = false;       // track if blaster firing
boolean isBurst                   = false;       // track selector switch behavior for burst/full.        

unsigned long timerSolenoidDetect = 0;
boolean       isSolenoidExtended  = false;
float   battVoltage;
unsigned long timer               = 0;

//declare display 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Declare and Instantiate Bounce objects
Bounce btnRev            = Bounce(); 
Bounce btnTrigger        = Bounce(); 
Bounce switchSelector    = Bounce();
Bounce switchButton      = Bounce();



// Function: shotFiredHandle

void shotFiringHandle() {
  if(fireMode == MODE_SINGLE){
    delaySolenoidRetracted = MINSOLENOIDDELAY;
  } else {
    delaySolenoidRetracted = MINSOLENOIDDELAY + delayOffset;
  }
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
      } else if ((millis() - timerSolenoidDetect) >= delaySolenoidRetracted) {
        digitalWrite(PIN_SOLENOID, HIGH); // Extend Solenoid
        isSolenoidExtended = true;
        timerSolenoidDetect = millis();
      }      
    }
  }
}


// Function: triggerPressedHandle

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


// Function: triggerReleasedHandle

void triggerReleasedHandle() {  
  if (((modeFire == MODE_AUTO) || (modeFire == MODE_BURST)) && isFiring && (dartToBeFire > 1)) {
      dartToBeFire = 1;    // fire off last shot
  }
}


// Function: readVoltage

void readVoltage() {
  // you might have to adjust the formula according to the voltage sensor you use
  battVoltage = (analogRead(PIN_VOLTREAD) * 0.245); //converts digital to a voltage
  //0.0245 comes from fixed point calculation of (5/1024) * 5 * 10
}


// Function: updateDisplay

void updateDisplay() {
  
  readVoltage();
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(battVoltage/10,1);
  display.setTextSize(1);
  switch(modeFire){
    case(MODE_SINGLE):
    display.println("SEMI");
    break;
    case(MODE_BURST):
    display.println("BURST");
    break;
    case(MODE_AUTO):
    display.println("AUTO");
    break;
  }
  display.print("ROF: ");
  display.println(delayOffset);
  display.print("Burst Limit: ");
  display.println(burstLimit);
  display.print("PWM: ");
  display.println(PWMSetting);
  display.display();
}




// Function: main menu

void menu(){
   display.setTextSize(1);
   display.setTextColor(WHITE);
   for (int i = 0; i <= 50; i += 10){
    display.setCursor(0,i);
    display.println(menus[i/10]);
   }
   display.display();
   unsigned long menuTime  = millis();
   boolean showCursor = true;
   int menuCursor = 10;
          
   while(1){
          if (millis() - menuTime >= 500){
             menuTime = millis();
             if (showCursor){
              showCursor = false;
              display.drawRect(0,menuCursor,128,10, WHITE);
             } else {
              showCursor = true;
              display.clearDisplay();
              for (int i = 0; i <= 50; i += 10){
              display.setCursor(0,i);
              display.println(menus[i/10]);
              }

              display.setCursor(0,menuCursor);
              display.println(menus[menuCursor/10]);
             }
             display.display();
          }
          
          btnTrigger.update(); //trigger acts as "confirm selection"
          switchButton.update(); //button acts as "change value"
          if (switchButton.fell()){
              display.setCursor(0,menuCursor);
              display.println(menus[menuCursor/10]);
            if (menuCursor == 50){
              menuCursor = 10;
              menuTime -= 500;
            } else{
              menuCursor += 10;
              menuTime -= 500; //so it updates the cursor location
            }
          }
          if (btnTrigger.fell()){
            nextState = menuCursor/10;
            break;
          }
   }
   
}

// Function: Change Values

void changeValue(){
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.println(menus[currentState]);
            switch(currentState){
            case (ROF):


            display.setCursor(0,10);
            display.println("Max value: 100");
            display.println("Min Value: 0");
            //display.setCursor(0,30);
            display.print(delayOffset);
            break;
            case (AUTO_BURST):
            display.setCursor(0,10);
            display.println("Burst: True");
            display.println("Auto: False");
            //display.setCursor(0,30);
            display.print(isBurst ? "True" : "False");
            break;
            case (BURST_LIMIT):
            display.setCursor(0,10);
            display.println("Max value: 10");
            display.println("Min Value: 2");
            //display.setCursor(0,30);
            display.print(burstLimit);
            display.display();
            break;
            case (PWM):
            display.setCursor(0,10);
            display.println("Max value: 100");
            display.println("Min Value: 50");
            display.println(PWMSetting);
            break;
            }
   display.display();
   
   while(1){
          btnTrigger.update(); //trigger acts as "confirm selection"
          switchButton.update(); //button acts as "change value"
          if (switchButton.fell()){
            switch(currentState){
            case (ROF):
            display.clearDisplay();
            display.setCursor(0,0);
            display.println(menus[currentState]);
            //display.setCursor(0,10);
            display.println("Max value: 100");
            display.println("Min Value: 0");
            //display.setCursor(0,30);
            delayOffset += 10;
            if (delayOffset > 100){
              delayOffset = 0;
            }
            display.print(delayOffset);
            display.display();
            break;
            case (AUTO_BURST):
            display.clearDisplay();
            display.setCursor(0,0);
            display.println(menus[currentState]);
            //display.setCursor(0,10);
            display.println("Burst: True");
            display.println("Auto: False");
           // display.setCursor(0,30);
            display.print(isBurst ? "True" : "False");
            if (isBurst){
              isBurst = false;
            } else {
              isBurst = true;
            }
            display.display();
            break;
            case (BURST_LIMIT):
            display.clearDisplay();
            display.setCursor(0,0);
            display.println(menus[currentState]);
           // display.setCursor(0,10);
            display.println("Max value: 10");
            display.println("Min Value: 2");
            //display.setCursor(0,30);
            burstLimit++;
            if (burstLimit > 10){
              burstLimit = 2;
            }
            display.print(burstLimit);
            display.display();
            break;
            case (PWM):
            display.clearDisplay();
            display.setCursor(0,0);
            display.println(menus[currentState]);
            //display.setCursor(0,10);
            display.println("Max value: 100");
            display.println("Min Value: 50");
            PWMSetting++;
            if (PWMSetting >100){
              PWMSetting = 50;
            }
            display.println(PWMSetting);
            display.display();
            break;
            }
          }
          if (btnTrigger.fell()){
            nextState = 0;
            break;
          }
   }
   
}





void setup() { // initilze  
  //setup vars
  boolean setupBlaster = true;
  int     currentState              = 4;
  int     nextState                 = 0;
  String menus [] = {"MAIN MENU:", "Rate of fire","AUTO/BURST","Burst Settings","PWM Settings","Save and exit"};
  
  //initialize all timers except for 0, to save time keeping functions
  InitTimersSafe();
  //sets up the setup loop if trigger is pulled

  //sets the frequency for the specified pin
  bool success = SetPinFrequencySafe(PIN_FLYWHEEL_MOSFET, frequency);
  
  // if the pin frequency was set successfully, turn pin 13 on, a visual check
  // can be commented away in final upload to arduino board
  if(success) {
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);    
  }
  
  // INPUT PINs setup
  // Note: Most input pins will be using internal pull-up resistor. A fall in signal indicate button pressed.

   
  pinMode(PIN_REV,INPUT_PULLUP);              // PULLUP
  btnRev.attach(PIN_REV);
  btnRev.interval(5);                         //debounce period
    
  pinMode(PIN_DARTTRIGGER,INPUT_PULLUP);      // PULLUP
  btnTrigger.attach(PIN_DARTTRIGGER);
  btnTrigger.interval(5);

  pinMode(PIN_SELECTOR,INPUT_PULLUP);     // PULLUP
  switchSelector.attach(PIN_SELECTOR);
  switchSelector.interval(5);

  pinMode(PIN_BUTTON,INPUT_PULLUP);     // PULLUP
  switchButton.attach(PIN_BUTTON);
  switchButton.interval(5);

  pinMode(PIN_VOLTREAD, INPUT);               // Not using PULLUP analog read 0 to 1023


  // OUTPUT PINs setup

  pinMode (PIN_FLYWHEEL_MOSFET, OUTPUT);
  pwmWrite(PIN_FLYWHEEL_MOSFET, 0);  
  pinMode(PIN_SOLENOID, OUTPUT);
  digitalWrite(PIN_SOLENOID, LOW);
  
  
  if (digitalRead(PIN_SELECTOR) == LOW){
    modeFire = MODE_AUTO;
  } else {
    modeFire = MODE_SINGLE;
  }       

  
  fwSpeed =  map(PWMSetting , 0, 100, 0, 255);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.clearDisplay();

  if (digitalRead(PIN_DARTTRIGGER) == LOW){
    while(setupBlaster){
          switch(nextState){
            case (MAIN_MENU):
            if (nextState != currentState){
              currentState = nextState;
              menu();
            }
            break;
            case (5):
            setupBlaster = false; //exit case
            break;
            default:
              if (nextState != currentState){
              currentState = nextState;
              changeValue();
            }    

          }

    }
  }

  updateDisplay();
  timer = millis();
}


// Function: loop

void loop() { // Main Loop  
    

    
    btnRev.update();
    btnTrigger.update();
    switchSelector.update();
    //switchButton.update();    


    // Listen to Rev Press/Release

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
  
 
    // Listen to Trigger Pull/Release

    if (btnTrigger.fell()) {               // pull
        triggerPressedHandle(modeFire);        
    } else if (btnTrigger.rose()) {        // released
        triggerReleasedHandle();
    }
  

    // Listen to Firing

    shotFiringHandle();

    // Listen to Firing Mode change: Single Shot, Burst, Full Auto

    if (switchSelector.changed()) {
        timer -= 6000; //update display  
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

    if (millis() - timer > 5000){
      updateDisplay();
      timer = millis();    
    }

}
