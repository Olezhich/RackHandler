#include <avr/wdt.h>

// config 

#define BAUD_RATE 115200
#define BUTTON_PIN 3
#define LED_PIN LED_BUILTIN
#define RELAY_PIN1 4
#define RELAY_PIN2 5

#define PRESSED_TRESHHOLD 14
#define RELEASE_TRESHHOLD 2

#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define SHORT_PRESS_MIN 100
#define LONG_PRESS_MIN 1000
#define LONG_PRESS_MAX 10000

#define POWEROFF_TIMEOUT 20000

#define LED_ON_TIMEOUT 10000
#define LED_BLINK_TIMEOUT 5000
#define LED_BLINK_SLOW_DELTA 500
#define LED_BLINK_FAST_DELTA 200

typedef struct{
  uint32_t SysTick;
  bool ButtonPressed;
  bool ButtonStateChanged;
  uint32_t PressTime;
  uint32_t ReleaseTime;
} ButtonState;

volatile ButtonState btn = {0};

volatile uint16_t ButtonIntegrator = 0x0;

uint32_t SystemOnTime = 0;

enum class SystemState : uint8_t { OFF, ON, FAULT };
SystemState CurrentState = SystemState::OFF;

bool led_blink(uint32_t delta, uint32_t current_time){
  return (current_time % delta) < (delta / 2);
}

void relay_handler(bool relay_state){
  digitalWrite(RELAY_PIN1, relay_state);
  digitalWrite(RELAY_PIN2, relay_state);
}

void setup() {
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
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);

  relay_handler(RELAY_OFF);

  wdt_enable(WDTO_2S);

  Serial.println("___System ready___");
}

void loop() {
  // watchdog reset
  
  if(CurrentState != SystemState::FAULT){
    wdt_reset();
  }else{
    relay_handler(RELAY_OFF);
    return;
  }

  // get context
  
  ButtonState ctx;
  
  noInterrupts();
  ctx.SysTick =  btn.SysTick;
  ctx.ButtonPressed = btn.ButtonPressed;
  ctx.ButtonStateChanged = btn.ButtonStateChanged;
  ctx.PressTime = btn.PressTime;
  ctx.ReleaseTime = btn.ReleaseTime;
  btn.ButtonStateChanged = false;
  interrupts(); 

  // Обработка залипания кнопки

  if(ctx.ButtonPressed && ctx.SysTick - ctx.PressTime > LONG_PRESS_MAX) {
    CurrentState = SystemState::FAULT;
    Serial.println("FAULT");
    return;
  }

  // Обработка изменения состояния кнопки

  if(ctx.ButtonStateChanged){
    // Вся логика работает по факту отпускания кнопки
    if(!ctx.ButtonPressed){
      uint32_t PressDuration = ctx.ReleaseTime - ctx.PressTime;

      // Обработка короткого нажатия
      if(PressDuration > SHORT_PRESS_MIN && PressDuration <= LONG_PRESS_MIN){
        SystemOnTime = ctx.SysTick;
        Serial.print("Timer Update: ");
        Serial.println(SystemOnTime);
      // Обработка длинного нажатия
      }else if(PressDuration > LONG_PRESS_MIN && PressDuration <= LONG_PRESS_MAX){
        if(CurrentState == SystemState::OFF){
          CurrentState = SystemState::ON;
          relay_handler(RELAY_ON);
          SystemOnTime = ctx.SysTick;
          Serial.print("Relay ON: ");
          Serial.println(SystemOnTime);
        }else{
          CurrentState = SystemState::OFF;
          relay_handler(RELAY_OFF);
          Serial.print("Relay OFF: ");
          Serial.println(ctx.SysTick);
        }
      // Обработка залипания кнопки
      }else if(PressDuration > LONG_PRESS_MAX){
        CurrentState = SystemState::FAULT;
        Serial.println("FAULT");
        return;
      }
    }
  }

  // Обработка таймера отключения
  uint32_t PassedTime = ctx.SysTick - SystemOnTime;
  
  if(CurrentState == SystemState::ON && PassedTime > POWEROFF_TIMEOUT){
    CurrentState = SystemState::OFF;
    relay_handler(RELAY_OFF);
    Serial.print("Relay OFF: ");
    Serial.println(ctx.SysTick);
  }

  // Обработка светодиода
  if(CurrentState != SystemState::ON){
    digitalWrite(LED_PIN, LOW);
  }else{
    uint32_t RemainingTime = POWEROFF_TIMEOUT - PassedTime;

    if(RemainingTime > LED_ON_TIMEOUT){
      digitalWrite(LED_PIN, HIGH);
    }else if(RemainingTime > LED_BLINK_TIMEOUT){
      digitalWrite(LED_PIN, led_blink(LED_BLINK_SLOW_DELTA, ctx.SysTick));
    }else{
      digitalWrite(LED_PIN, led_blink(LED_BLINK_FAST_DELTA, ctx.SysTick));
    }
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
