/*===================[LICENSE]===================

NumenHand – GNU Affero General Public License v3.0
Copyright (C) 2025  Manu Montaraz

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

===============================================*/

#include <SPI.h>
#include <BleMouse.h>

BleMouse bleMouse;
#define MOT_PIN 9
#define SCLK_PIN 4
#define MOSI_PIN 6
#define MISO_PIN 5
#define CS_PIN 7
#define NRESET_PIN 8
#define LCLIC_PIN 10
#define MCLIC_PIN 3
#define RCLIC_PIN 2

unsigned long lastSend = 0;
const unsigned long interval = 5; // ms entre envíos al PC

const uint8_t millisToPress = 0;
unsigned long startTimeButtonPress = 0;
unsigned long timeButtonPress = 0;
uint8_t buttonPressing = 0; // 1 = izquierdo | 2 = central | 3 = derecho
uint8_t lastButtonPressing = 0;
bool pressed = false;

void setup() {
  Serial.begin(115200);
  bleMouse.begin();

  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  pinMode(LCLIC_PIN, INPUT);
  pinMode(MCLIC_PIN, INPUT);
  pinMode(RCLIC_PIN, INPUT);

  initPAW3395();
}

const float DPI_SCALE = 0.1; // Ajusta entre 0.0 y 1.0 (más bajo = más lento)

void loop() {
  if (bleMouse.isConnected()) {
    static int16_t dx_accum = 0;
    static int16_t dy_accum = 0;

    int16_t dx = 0, dy = 0;
    if(readPAW3395(dx, dy)) {
      dx = -dx; // Corregir inversión horizontal
      dx_accum += dx;
      dy_accum += dy;
    }

    unsigned long now = millis();
    if (now - lastSend >= interval) {
      // Filtrar ruido
      const int MIN_MOVEMENT = 2;
      if(abs(dx_accum) < MIN_MOVEMENT) dx_accum = 0;
      if(abs(dy_accum) < MIN_MOVEMENT) dy_accum = 0;

      if(dx_accum != 0 || dy_accum != 0) {
        int dx_scaled = (int)round(dx_accum * DPI_SCALE);
        int dy_scaled = (int)round(dy_accum * DPI_SCALE);

        // Evitar perder pequeños movimientos escalados
        if(dx_scaled == 0 && dx_accum != 0) dx_scaled = (dx_accum > 0 ? 1 : -1);
        if(dy_scaled == 0 && dy_accum != 0) dy_scaled = (dy_accum > 0 ? 1 : -1);

        bleMouse.move(dx_scaled, dy_scaled);

        Serial.print("dx: "); Serial.print(dx_scaled);
        Serial.print(" dy: "); Serial.println(dy_scaled);

        dx_accum = 0;
        dy_accum = 0;
      }

      lastSend = now;
    }

    check_clic_button();
    
  }
}

void check_clic_button(){
  //Serial.println("LCLIC: "+String(digitalRead(LCLIC_PIN)));
  //Serial.println("MCLIC: "+String(digitalRead(MCLIC_PIN)));
  //Serial.println("RCLIC: "+String(digitalRead(RCLIC_PIN)));

  const uint8_t millisCancelScroll = 250;

  static uint8_t positions = 0;

  static unsigned long startTimeLeft = 0;
  static unsigned long timeLeft = 0;
  static uint8_t positionLeft = 0;
  static bool pressingLeft = false;
  
  static unsigned long startTimeMiddle = 0;
  static unsigned long timeMiddle = 0;
  static uint8_t positionMiddle = 0;
  static bool pressingMiddle = false;

  static unsigned long startTimeRight = 0;
  static unsigned long timeRight = 0;
  static uint8_t positionRight = 0;
  static bool pressingRight = false;

  if(digitalRead(LCLIC_PIN)){
    if(startTimeLeft == 0){
      startTimeLeft = millis();
      positions++;
      positionLeft = positions;
    }
    timeLeft = millis() - startTimeLeft;
    pressingLeft = true;
  }else{
    pressingLeft = false;
  }

  if(digitalRead(MCLIC_PIN)){
    if(startTimeMiddle == 0){
      startTimeMiddle = millis();
      positions++;
      positionMiddle = positions;
    }
    timeMiddle = millis() - startTimeMiddle;
    pressingMiddle = true;
  }else{
    pressingMiddle = false;
  }
  
  if(digitalRead(RCLIC_PIN)){
    if(startTimeRight == 0){
      startTimeRight = millis();
      positions++;
      positionRight = positions;
    }
    timeRight = millis() - startTimeRight;
    pressingRight = true;
  }else{
    pressingRight = false;
  }
  
  if(!digitalRead(LCLIC_PIN) && !digitalRead(MCLIC_PIN) && !digitalRead(RCLIC_PIN)){
    if(positions != 0){
      positions = 0;
    }
    
    if(positionLeft != 0){
      if(positionLeft == 2 && positionMiddle == 1 && millisCancelScroll > timeMiddle){
        bleMouse.move(0, 0, 2);
      }

      positionLeft = 0;
      startTimeLeft = 0;
      timeLeft = 0;
    }

    if(positionRight != 0){
      if(positionRight == 2 && positionMiddle == 1 && millisCancelScroll > timeMiddle){
        bleMouse.move(0, 0, -2);
      }

      positionRight = 0;
      startTimeRight = 0;
      timeRight = 0;
    }

    if(positionMiddle != 0){
      positionMiddle = 0;
      startTimeMiddle = 0;
      timeMiddle = 0;
    }

    bleMouse.release(MOUSE_LEFT);
    bleMouse.release(MOUSE_MIDDLE);
    bleMouse.release(MOUSE_RIGHT);
  }

  Serial.println("LCLIC: "+String(positionLeft)+" | "+String(timeLeft)+" | "+String(pressingLeft));
  Serial.println("MCLIC: "+String(positionMiddle)+" | "+String(timeMiddle)+" | "+String(pressingMiddle));
  Serial.println("RCLIC: "+String(positionRight)+" | "+String(timeRight)+" | "+String(pressingRight));

  if(positionLeft == 1 || (positionRight == 1 && pressingLeft)){
    bleMouse.press(MOUSE_LEFT);
  }else if(positionRight == 1 && !pressingLeft){
    bleMouse.release(MOUSE_LEFT);
  }
    
  if(positionRight == 1){
    bleMouse.press(MOUSE_RIGHT);
  }
  
  if(positionMiddle == 1 && millisCancelScroll <= timeMiddle){
    bleMouse.press(MOUSE_MIDDLE);
  }

}

// Inicialización básica del PAW3395
void initPAW3395() {
  digitalWrite(CS_PIN, LOW);
  delay(5); // pequeño delay al despertar el sensor
  // Reset, modo normal, DPI, etc. según datasheet
  // Por ejemplo:
  // writeRegister(0x3A, 0x5A); // reset
  // delay(10);
  digitalWrite(CS_PIN, HIGH);
}

// Escritura de registro
void writeRegister(uint8_t reg, uint8_t val) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg | 0x80); // MSB=1 para escribir
  SPI.transfer(val);
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(20);
}

// Leer movimiento, devuelve true si hubo desplazamiento
bool readPAW3395(int16_t &dx, int16_t &dy) {
  dx = 0;
  dy = 0;

  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x02); // Motion register
  uint8_t motion = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);

  // Solo si hay movimiento real
  if (motion & 0x80) {
      digitalWrite(CS_PIN, LOW);

      // Delta X
      SPI.transfer(0x03);
      uint8_t dx_low = SPI.transfer(0x00);
      delayMicroseconds(5);
      SPI.transfer(0x04);
      uint8_t dx_high = SPI.transfer(0x00);
      delayMicroseconds(5);

      // Delta Y
      SPI.transfer(0x05);
      uint8_t dy_low = SPI.transfer(0x00);
      delayMicroseconds(5);
      SPI.transfer(0x06);
      uint8_t dy_high = SPI.transfer(0x00);

      digitalWrite(CS_PIN, HIGH);

      dx = (int16_t)((dx_high << 8) | dx_low);
      dy = (int16_t)((dy_high << 8) | dy_low);

      return true;
  }

  return false; // no hubo movimiento, así no se acumula ruido
}