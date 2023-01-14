//Functional generator AD9833 centric
//with IR control
//G Godkin
//Initial version Jan 13, 2023

#define maxFreq 12500000
//DEBUG 0 for production, 1 for development
#define DEBUG 1
//#define DEBUG 0

#if DEBUG == 0
  #define serial Serial
  #define debugln(x) Serial.println(x) 
  #define debug(x) Serial.print(x) 
  #define debugBegin(x) Serial.begin(x)
#else
  #define serial true
  #define debugln(x)
  #define debug(x)
  #define debugBegin(x)
#endif

#include <MD_AD9833.h>
#include <SPI.h>
#include <Arduino.h>
#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.
#include <IRremote.hpp>

#include <Wire.h>
#include <hd44780.h>						// main hd44780 header
#include <hd44780ioClass/hd44780_I2Cexp.h>	// i2c expander i/o class header

hd44780_I2Cexp lcd; // declare lcd object: auto locate & config exapander chip
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

#define DECODE_NEC          // Includes Apple and Onkyo
#define UPDATE_FREQ 500

#include <MD_AD9833.h>
#include <SPI.h>

// Pins for SPI comm with the AD9833 IC
#define DATA  11	///< SPI Data pin number
#define CLK   13	///< SPI Clock pin number
#define FSYNC 10	///< SPI Load pin number (FSYNC in AD9833 usage)

MD_AD9833	AD(FSYNC);  // Hardware SPI
// MD_AD9833	AD(DATA, CLK, FSYNC); // Arbitrary SPI pins
static MD_AD9833::mode_t mode = MD_AD9833::MODE_SINE;
static unsigned long freq;
static unsigned int pass;

void setup() {
    debugBegin(115200);
    // Just to know which program is running on my Arduino
    debugln(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
    int status;
    status = lcd.begin(LCD_COLS, LCD_ROWS);
    if(status) // non zero status means it was unsuccesful
    {
      // begin() failed so blink error code using the onboard LED if possible
      hd44780::fatalError(status); // does not return
    }

    // Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards definition as default feedback LED
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
	  AD.begin();

    debug(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    debugln(F("at pin " STR(IR_RECEIVE_PIN)));
    lcd.print(F("Ready to receive IR signals of protocols: "));
    lcd.print(F("at pin " STR(IR_RECEIVE_PIN)));
    delay(2000);
    lcd.clear();
}

String getMode (MD_AD9833::mode_t gMode) {
  switch (gMode) {
    case MD_AD9833::MODE_SINE:
      return " SINE";
      break;
    case MD_AD9833::MODE_SQUARE1:
      return " SQR ";
      break;
    case MD_AD9833::MODE_TRIANGLE:
      return " TRNG";
      break;
  }  
}

signed int irDecoder( signed long ir){
  switch (ir) {
    case 0:
      return 1;
      break;
    case 7:
      return 2;
      break;
    case 6:
      return 3;
      break;
    case 4:
      return 4;
      break;
    case 11:
      return 5;
      break;
    case 10:
      return 6;
      break;
    case 8:
      return 7;
      break;
    case 15:
      return 8;
      break;
    case 14:
      return 9;
      break;
    case 19:
      return 0;
      break;

    case 17:
      return -10; //Exit - reset
      break;
    case 21:
      return -11; //Ok
      break;
      
    case 3:
      return -15; //Sine wave
      break;
      
    case 28:
      return -16; //Triangle wave
      break;
      
    case 12:
      return -17; //meandr
      break;
    return -100;  
  }
}

void setGenerator(signed long lcmd){
  debug("Entering decoder with lcmd: ");
  debugln(lcmd);
     if (lcmd != -1) {
      if (irDecoder(lcmd) < -1) {
        unsigned int nextChan;
        unsigned int currentChan = AD.getMode();
        if (currentChan == MD_AD9833::CHAN_0){
          nextChan = MD_AD9833::CHAN_1;
        } else { 
          nextChan = MD_AD9833::CHAN_0;
        }
        switch (irDecoder(lcmd)){
          case -10:     //Reset
            AD.setMode(MD_AD9833::MODE_OFF);          
            lcd.clear();
            lcd.setCursor(0,0);
            pass = 0;
            freq = 0;
            break;            
          case -11:     //Ok
            AD.setMode(mode);
            AD.setPhase(nextChan, 0);
            AD.setFrequency(nextChan, freq);
            AD.setActiveFrequency(nextChan);
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print(freq);
            lcd.print(" Hz");
            lcd.print(getMode(mode));
            pass = 0;
            freq = 0;
            break;
                    
          case -15:       //Sine
            mode = MD_AD9833::MODE_SINE;
            lcd.setCursor(0,0);
            lcd.print(freq);
            lcd.print(" Hz");
            lcd.print(getMode(mode));
            break;
          case -16:       //Triangle
            mode = MD_AD9833::MODE_TRIANGLE;
            lcd.setCursor(0,0);
            lcd.print(freq);
            lcd.print(" Hz");
            lcd.print(getMode(mode));
            break;
          case -17:       //Meandr
            mode = MD_AD9833::MODE_SQUARE1;
            lcd.setCursor(0,0);
            lcd.print(freq);
            lcd.print(" Hz");
            lcd.print(getMode(mode));
            break;
        }
        // lastCommand = -1;
      }  else {
        if (lcmd >= 0) {
          debug("pass before ++: ");
          debugln((pass));
          pass ++;
          switch (pass){
            case 1:
              freq = irDecoder(lcmd);
              lcd.setCursor(0,0);
              lcd.print(freq);
              lcd.print(" Hz");
              lcd.print(getMode(mode));
              break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
              lcd.setCursor(0,0);
              freq = freq * 10 + irDecoder(lcmd);
              if (freq > maxFreq){
                freq = maxFreq;
              }
              lcd.print(freq);
              lcd.print(" Hz");
              lcd.print(getMode(mode));
              break;
          }
        }
      }
      // debug("Last Command decoded: ");
      // debugln(irDecoder(lcmd));
      // debug("freq: ");
      // debugln((freq));
      // debug("pass: ");
      // debugln((pass));
    }
   }

void loop() {
  static unsigned long timeCapture = 0;
  static signed long lastCommand;
  if (IrReceiver.decode()) {

      // Print a short summary of received data
      // IrReceiver.printIRResultShort(&Serial);
      // IrReceiver.printIRSendUsage(&Serial);
      lastCommand = IrReceiver.decodedIRData.command;
      Serial.print("Received: ");
      Serial.println(lastCommand);
      if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
          Serial.println(F("Received noise or an unknown (or not yet enabled) protocol"));
          // We have an unknown protocol here, print more info
          IrReceiver.printIRResultRawFormatted(&Serial, true);
      }
      // Serial.println();

      /*
        * !!!Important!!! Enable receiving of the next value,
        * since receiving has stopped after the end of the current received data packet.
        */
      IrReceiver.resume(); // Enable receiving of the next value
  }

  if (lastCommand != -1) {
    debug("P1: ");
    debugln(lastCommand);
    if ((millis()-timeCapture) > (UPDATE_FREQ )) {
      timeCapture = millis();
      debug("P2: ");
      debugln(lastCommand);
      setGenerator(lastCommand);
    }
   }
  lastCommand = -1;
}

