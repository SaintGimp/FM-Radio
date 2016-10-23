#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// PT2257 code borrowed from https://github.com/victornpb/Evc_pt2257
// Note that the datasheet gives the address as 0x88 but the Wire library
// uses 7-bit addresses so we shift it once to the right
#define PT2257_ADDR 0x44        //Chip address
#define EVC_OFF     0b11111111  //Function OFF (-79dB)
#define EVC_2CH_1   0b11010000  //2-Channel, -1dB/step
#define EVC_2CH_10  0b11100000  //2-Channel, -10dB/step
#define EVC_L_1     0b10100000  //Left Channel, -1dB/step
#define EVC_L_10    0b10110000  //Left Channel, -10dB/step
#define EVC_R_1     0b00100000  //Right Channel, -1dB/step
#define EVC_R_10    0b00110000  //Right Channel, -10dB/step
#define EVC_MUTE    0b01111000  //2-Channel MUTE

#define MIN_ATTENUATION 0
#define MAX_ATTENUATION 78
#define ATTENUATION_STEP 1

#define ENCODER_A 2
#define ENCODER_B 3
#define ENCODER_BUTTON_PIN 4

// set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x3F,16,2);

double frequency;
unsigned char frequencyH = 0;
unsigned char frequencyL = 0;
unsigned int frequencyB;
int attenuation;
boolean mode;

volatile int turnsDetected;
volatile boolean up;
volatile unsigned long lastTurn;

byte customChar[8] = {  //creates the arrow 
  0b10000,
  0b11000,
  0b11100,
  0b11110,
  0b11110,
  0b11100,
  0b11000,
  0b10000
};

void isr0 ()  {
  turnsDetected++;
  unsigned long now = millis();
  if (now - lastTurn > 20) { 
    up = (digitalRead(ENCODER_A) == digitalRead(ENCODER_B));
  }
  lastTurn = now;
}

void setFrequency()  {
  frequencyB = 4 * (frequency * 1000000 + 225000) / 32768;
  frequencyH = frequencyB >> 8;
  frequencyL = frequencyB & 0XFF;
  Wire.beginTransmission(0x60);
  Wire.write(frequencyH);
  Wire.write(frequencyL);
  Wire.write(0xB0);
  Wire.write(0x10);
  Wire.write((byte)0x00);
  Wire.endTransmission(); 
} 

byte evc_level(uint8_t dB) {   
  if (dB>79)
    dB=79;
  
  uint8_t b = dB/10;  //get the most significant digit (eg. 79 gets 7)
  uint8_t a = dB%10;  //get the least significant digit (eg. 79 gets 9)
  
  b = b & 0b0000111; //limit the most significant digit to 3 bit (7)
  
  return (b<<4) | a; //return both numbers in one byte (0BBBAAAA)
}

void setVolume(uint8_t dB) {
  byte bbbaaaa = evc_level(dB);
  
  byte aaaa = bbbaaaa & 0b00001111;
  byte bbb = (bbbaaaa>>4) & 0b00001111;
  
  Wire.beginTransmission(PT2257_ADDR);
  Wire.write(EVC_2CH_10 | bbb);
  Wire.write(EVC_2CH_1 | aaaa);
  Wire.endTransmission(); 
}

void mute(bool enable) {
  Wire.beginTransmission(PT2257_ADDR);
  Wire.write(EVC_MUTE | (enable & 0b00000001));
  Wire.endTransmission(); 
}

void displaydata(){
  arrow();
  
  lcd.setCursor(1,0);
  lcd.print("FM: ");
  lcd.print(frequency);
  lcd.print("   ");
  lcd.setCursor(1,1);
  lcd.print("Vol: -");
  lcd.print(attenuation);
  lcd.print("db   ");
} 

void arrow(){
  lcd.setCursor(0,0);
  if (mode == 1) {
    lcd.write((uint8_t)0);
  }
  else {
    lcd.print(" ");
  } 

  lcd.setCursor(0,1);
  if (mode == 1) {
    lcd.print(" ");
  }
  else {
    lcd.write((uint8_t)0);
  } 
}

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, customChar); // arrow Char created

  Wire.begin();
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);  
  pinMode(ENCODER_BUTTON_PIN, INPUT);

  delay(200);
  attenuation = 50; //starting Volume
  setVolume(attenuation);
  mute(false);

  mode = 1; //frequency mode
  frequency = 94.1; //starting Frequency
  setFrequency();

  displaydata();
  attachInterrupt (0, isr0, FALLING);
}

void loop() {
  if (digitalRead(ENCODER_BUTTON_PIN)) {
    mode = !mode;
    displaydata();
    delay(500);
  }
    
  if (turnsDetected && mode == 1) {
    if (up) {
      frequency = frequency + (0.1 * turnsDetected);
      if (frequency >= 107.90) {
        frequency = 107.9;
      }
    }
    else {
      frequency = frequency - (0.1 * turnsDetected);
      if (frequency <= 87.6) {
        frequency = 87.6;
      }
    }
    
    turnsDetected = 0;
    setFrequency();
    displaydata();
  }

  if (turnsDetected && mode == 0) {
    if (up) {
      attenuation = attenuation - (ATTENUATION_STEP * turnsDetected);
      if (attenuation <= MIN_ATTENUATION) {
        attenuation = MIN_ATTENUATION;
      }
    }
    else {
      attenuation = attenuation + (ATTENUATION_STEP * turnsDetected);
      if (attenuation >= MAX_ATTENUATION) {
        attenuation = MAX_ATTENUATION;
      }
    }   

    turnsDetected = 0;
    setVolume(attenuation);
    displaydata();
  }
}
