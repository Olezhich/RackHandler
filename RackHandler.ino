#include <avr/wdt.h>

volatile uint16_t ButtonIntegrator = 0x0;
volatile bool ButtonPressed = false;
volatile bool ButtonStateChanged = false;

#define BAUD_RATE 115200
#define BUTTON_PIN 2
#define PRESSED_TRESHHOLD 14

void printBinary(uint32_t value, uint8_t bits) {
    for (int8_t i = bits - 1; i >= 0; i--) {
        Serial.print((value >> i) & 1);
    }
    Serial.println();
}

void setup() {
  // put your setup code here, to run once:

  pinMode(LED_BUILTIN, OUTPUT);

  noInterrupts();

  TCCR1A = 0; //регистр управления для таймера 1
  TCCR1B = 0; //регистр управления для таймера 1
  TCNT1  = 0; //регитср аккумулятор для таймера 1

  OCR1A = 25000; //регистр сравнения для таймера 1, хранит целевое значение тиков, которое длолджно пройти до вызова прерывания

  TCCR1B |= (1 << WGM12); //режим CTC (Clear Timer on Compare), таймер досчитывает до числа определенного в OCR1A и сбразывается в 0, вызывая прерывание 

  TCCR1B |= (1 << CS11) | (1 << CS10); //выстявляются биты отвечающие за делитель частоты, в данном случае на 64

  TIMSK1 |= (1 << OCIE1A); //разрешает прерывание

  interrupts();  

  Serial.begin(BAUD_RATE);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // put your main code here, to run repeatedly:

  if(ButtonStateChanged){
    noInterrupts();
    ButtonStateChanged = false;
    interrupts(); 
    Serial.println("ButtonPressed");
  }

}

ISR(TIMER1_COMPA_vect) {
  static uint8_t counter = 0;
  counter++;
  
  // Так как прерывание каждые 100 мс, то 10 раз = 1 секунда
  if (counter >= 10) {
    //делаем замедление в 10 раз для отладки

    uint8_t PinState = digitalRead(BUTTON_PIN);

    ButtonIntegrator = (ButtonIntegrator << 1) | PinState;

    bool NewState;
    if(__builtin_popcount(ButtonIntegrator) > PRESSED_TRESHHOLD){
      NewState = true;
    }else{
      NewState = false;
    }

    if(NewState != ButtonPressed){
      ButtonPressed = NewState;
      ButtonStateChanged = true;
    }else{
      ButtonStateChanged = false;
    }
    printBinary(ButtonIntegrator, 16);
  

    //конец
    
    counter = 0;
  }
  
}
