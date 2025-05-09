/*
Max Austin
CPE 301
5/9/2025
Final Project: Cooler
*/
#include "DHT.h"
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <Stepper.h>

#define DISABLED 0
#define IDLING 1
#define ERRORSTATE 2
#define RUNNING 3

const char* currentDate;
int runCount;
int state;
int currentTime;

// Water Reader
#define RDA 0x80
#define TBE 0x20

 // USART 0A Control and Status Register
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0 = (unsigned char *)0x00C6;
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;

volatile unsigned char *myADCSRB = (unsigned char*) 0x7B;
volatile unsigned int  *ADC_DATA = (unsigned int*)  0x78;
volatile unsigned char *myADMUX = (unsigned char*) 0x7C;
volatile unsigned char *myADCSRA = (unsigned char*) 0x7A;

volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a  = (unsigned char*) 0x21;

const float maxWtrV = 5.00;

#define RED 3
#define YLW 2
#define GRN 1
#define BLU 0
volatile unsigned char* port_c = (unsigned char*) 0x28;
volatile unsigned char* ddr_c = (unsigned char*) 0x27;

#define IN1 22
#define IN2 24
#define IN3 26
#define IN4 28
const int stepsPerRev = (2048 / 2);
const float maxPV = 5.00;
Stepper s(stepsPerRev, IN1, IN3, IN2, IN4);
int motorPosition;

#define DHTTYPE DHT11
#define DHTPIN 10

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
DHT dht(DHTPIN, DHTTYPE);

float temp;
float humid;
volatile unsigned int* ddrH = (unsigned int*)0x101;
volatile unsigned int* portH = (unsigned int*)0x102;

bool fanOn = false;

void lightOn(int);
void lightOff(int);
void checkVent(int);
void checkState(char);
int updateVent(void);
int getVent(void);
float checkWater(void);
void allLEDOff(void);
void ldcError(void);
void displayData(float, float);



void setup() 
{
  U0init(9600);
  delay(1000);
  printString("Setup\n");
  
  dht.begin();
  *ddrH |= 0x60;

  adc_init();

  s.setSpeed(5);
  s.step(0);

  *ddr_c |= 0b00001111;
  allLEDOff();

  *ddr_a |= 0b00000010;
  
  lcd.begin(16, 2);
  temp = 72.5911;
  humid = 25.8744;

  currentTime = 2401;
  currentDate = "13/31/2022";
  state = IDLING;

  printString("\nMain\n");
}

void loop() 
{

  switch (state) 
  {
  case DISABLED :
  checkState("DISABLED");
  while(state == DISABLED)
  {
    lightOn(YLW);
    fanTemp(0.00);
    checkVent(updateVent());
    if(state != DISABLED)
    {
      allLEDOff();
      break;
    }
  }
  break;

  case IDLING :
  checkState("IDLING");
  while(state == IDLING)
  {
    lightOn(GRN);
    if(checkWater() <= 0.75)
    {
      state = ERRORSTATE;
      allLEDOff();
      break;
    }
    humid = getHumid();
    temp = getTemp();
    displayData(temp, humid);
    if(fanTemp(temp))
    {
      state = RUNNING;
      allLEDOff();
      break;
    }
    checkVent(updateVent());
    if(state != IDLING)
    {
      allLEDOff();
      break;
    }
  }
  break;

  case ERRORSTATE :
  checkState("ERRORSTATE");
  while(state == ERRORSTATE)
  {
    lightOn(RED);
    fanTemp(0.00);
    ldcError();

    if(checkWater() > 0.75)
    {
      state = IDLING;
      allLEDOff();
      break;
    }
    humid = getHumid();
    temp = getTemp();
    displayData(temp, humid);
    checkVent(updateVent());
    if(state != ERRORSTATE)
    {
      allLEDOff();
      break;
    }
  }
  break;

  case RUNNING :
  checkState("RUNNING");
  while(state == RUNNING)
  {
    lightOn(BLU);
    if(checkWater() <= 0.75)
    {
      state = ERRORSTATE;
      allLEDOff();
      break;
    }
    humid = getHumid();
    temp = getTemp();
    if(!(fanTemp(temp)) )
    {
      state = IDLING;
      allLEDOff();
      break;
    }
    displayData(temp, humid);
    checkVent(updateVent());
    if(state != RUNNING)
    {
      allLEDOff();
      break;
    }
  }
  break;
 
  }
}

void checkState(const char* stateIn)
{
  printString("New state: ");
  printString(stateIn);
}

void printString(const char* stringIn)
{
  const char* text = stringIn;
  for(int i = 0; text[i] != '\0'; i++)
  {
    U0putchar(text[i]);
  }
}

void ldcError()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("     ERROR");
  lcd.setCursor(0, 1);
  lcd.print(" Water Low");
  delay(3000);
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("  Refill");
  delay(3000);
  
  lcd.setCursor(0, 0);
}

void displayData(float t, float h)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: "); lcd.print(t); lcd.print("*C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: "); lcd.print(h); lcd.print("%");
  delay(3000);
}

float checkWater(void)
{
  *port_a |= 0b00000010;
  delay(500);
  unsigned int adc_reading = adc_read(0);
  *port_a &= 0b11111101;
  float v = adc_reading * (maxWtrV / 1023.0);
  return v;
}

void adc_init()
{
  *myADCSRA |= 0b10000000;
  *myADCSRA &= 0b11011111;
  *myADCSRA &= 0b11011111;
  *myADCSRA &= 0b11011111;
 
  *myADCSRB &= 0b11110111;
  *myADCSRB &= 0b11111000;
  
  *myADMUX  &= 0b01111111;
  *myADMUX  |= 0b01000000;
  *myADMUX  &= 0b11011111;
  *myADMUX  &= 0b11011111;
  *myADMUX  &= 0b11100000;
}

unsigned int adc_read(unsigned char adc_channel)
{
  *myADCSRB &= 0b11110111;
  *myADMUX  &= 0b11100000;
  if(adc_channel > 7)
  {
    adc_channel -= 8;
    *myADCSRB |= 0b00001000;
  }
  *myADCSRA |= 0x40;
  *myADMUX  += adc_channel;
  while((*myADCSRA & 0x40) != 0);
  return *ADC_DATA;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

void lightOn(int num)
{
  switch (num)
  {
    case GRN:
      *port_c |= 0b00000010;
      break;
      
    case BLU:
      *port_c |= 0b00000001;
      break;
            
    case RED:
      *port_c |= 0b00001000;
      break;
      
    case YLW:
      *port_c |= 0b00000100;
      break;

  }
}

void lightOff(int num)
{
  switch (num)
  {
    case GRN:
      *port_c &= 0b11111101;
      break;
      
    case BLU:
      *port_c &= 0b11111110;
      break;
      
    case RED:
      *port_c &= 0b11110111;
      break;

    case YLW:
      *port_c &= 0b11111011;
      break;
  }
}

void allLEDOff(void)
{
  *port_c &= 0b11110000;
}

int updateVent(void)
{
  int potentValue;
  int desiredMotorPosition;
  int motorChange;

  if(state != DISABLED)
  {
    printString("Reading voltage\n");
    delay(1000);
    potentValue = analogRead(A8);
    float potentVoltage = potentValue * (maxPV / 1023.0);

    if(potentVoltage < 1)
    {
      desiredMotorPosition = 0;
    } else if(potentVoltage > 4)
    {
      desiredMotorPosition = stepsPerRev;
    } else 
    {
      desiredMotorPosition = ((stepsPerRev / maxPV) * potentVoltage);
    }
    motorChange = (desiredMotorPosition - motorPosition);

    s.step(motorChange);
    motorPosition = motorPosition + motorChange;
    delay(1000);
    return motorChange;
  } else {
    return 0;
  }
  
}

int getVent(void)
{
  return motorPosition;
}

void checkVent(int change)
{
  if(change != 0)
  {
    printString("Vent changed");
  } else{
    printString("No vent change\n");
  }
}

bool fanTemp(float tempC) 
{
  if (tempC >= 18) 
  {
    *portH |= 0x20;
    *portH |= 0x40;

    return true;
  } 
  else if (tempC < 18) 
  {
    *portH &= 0xDF;
    return false;
  }
}

float getHumid()
{
  float humi = dht.readHumidity();

  return humi;
}

float getTemp() 
{
  float tempC = dht.readTemperature();

  return tempC;
}
