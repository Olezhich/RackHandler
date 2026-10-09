#include <avr/wdt.h>

// config 

#define BAUD_RATE 115200
#define BUTTON_PIN 3
#define PRESSED_TRESHHOLD 14
#define RELEASE_TRESHHOLD 2

#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define SHORT_PRESS_MIN 100
#define LONG_PRESS_MIN 2000
#define LONG_PRESS_MAX 10000

typedef struct{
  uint32_t SysTick;
  bool ButtonPressed;
  bool ButtonStateChanged;
  uint32_t PressTime;
  uint32_t ReleaseTime;
} ButtonState;

volatile ButtonState btn = {0};

//volatile uint32_t SysTick = 0;
volatile uint16_t ButtonIntegrator = 0x0;
//volatile bool ButtonPressed = false;
//volatile bool ButtonStateChanged = false;
//volatile uint32_t PressTime = 0;
//volatile uint32_t ReleaseTime = 0;

enum class SystemState : uint8_t { OFF, ON, FAULT };

void setup() {
  // put your setup code here, to run once:

  pinMode(LED_BUILTIN, OUTPUT);

  noInterrupts();

  TCCR1A = 0; //регистр управления для таймера 1
  TCCR1B = 0; //регистр управления для таймера 1
  TCNT1  = 0; //регитср аккумулятор для таймера 1

  OCR1A = 249; //регистр сравнения для таймера 1, хранит целевое значение тиков, которое длолджно пройти до вызова прерывания

  TCCR1B |= (1 << WGM12); //режим CTC (Clear Timer on Compare), таймер досчитывает до числа определенного в OCR1A и сбразывается в 0, вызывая прерывание 

  TCCR1B |= (1 << CS11) | (1 << CS10); //выстявляются биты отвечающие за делитель частоты, в данном случае на 64

  TIMSK1 |= (1 << OCIE1A); //разрешает прерывание

  interrupts();  

  Serial.begin(BAUD_RATE);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  ButtonState ctx;
  
  noInterrupts();
  ctx.SysTick =  btn.SysTick;
  ctx.ButtonPressed = btn.ButtonPressed;
  ctx.ButtonStateChanged = btn.ButtonStateChanged;
  ctx.PressTime = btn.PressTime;
  ctx.ReleaseTime = btn.ReleaseTime;
  btn.ButtonStateChanged = false;
  interrupts(); 
  

  if(ctx.ButtonStateChanged){
    Serial.print(ctx.PressTime);
    Serial.print("   ");
    Serial.print(ctx.ReleaseTime);
    Serial.print("  Button State Changed to ");
    Serial.println(ctx.ButtonPressed);
    
  }

}

ISR(TIMER1_COMPA_vect) {
  btn.SysTick++;

  uint8_t PinState = !digitalRead(BUTTON_PIN);

  ButtonIntegrator = (ButtonIntegrator << 1) | PinState;
  uint8_t ButtonSum = __builtin_popcount(ButtonIntegrator);
  bool NewState = false;
  
  if(ButtonSum >= PRESSED_TRESHHOLD){
    NewState = true;
  }else if(ButtonSum <= RELEASE_TRESHHOLD){
    NewState = false;
  }else{
    return;
  }

  if(NewState != btn.ButtonPressed){
    btn.ButtonPressed = NewState;
    btn.ButtonStateChanged = true;
    if(NewState == true){
      btn.PressTime = btn.SysTick;
    }else{
      btn.ReleaseTime = btn.SysTick;
    }
  }
  
}
