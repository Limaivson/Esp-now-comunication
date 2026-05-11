#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 27
#define POT_PIN 32
#define BTN_CLEAR 14
#define BTN_MENU 12
#define LED_GREEN 19 
#define LED_RED 18   

Adafruit_SSD1306 display(128, 64, &Wire, -1);

typedef struct {
  int joyX, joyY;
  float speed_limit;
  int motor_count;
  int drive_mode;
  bool clear_errors;
} ControlPacket;

typedef struct {
  float bat1, bat2;
  int rpm1, rpm2;
  float temp1, temp2;
  float v_ms;
  bool system_ok;
  uint32_t uptime;
} TelemetryPacket;

ControlPacket tx;
TelemetryPacket rx;

uint8_t robotAddr[] = {0x08, 0xA6, 0xF7, 0x21, 0xBE, 0x60}; 
enum Menu { MAIN_DASH, TELEMETRY, LOGS_INFO };
Menu currentMenu = MAIN_DASH;

int offsetX = 0, offsetY = 0;
float lastPot = -1.0;
unsigned long potTimer = 0;
bool showSpeedOverlay = false;
unsigned long lastRecvTime = 0; 
int rssi_val = -100;


void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&rx, data, sizeof(rx));
  lastRecvTime = millis();
  rssi_val = WiFi.RSSI();
}

void setup() {
  Serial.begin(115200);
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BTN_CLEAR, INPUT_PULLUP);
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);

  long sx = 0, sy = 0;
  for(int i=0; i<50; i++) { sx += analogRead(JOY_X); sy += analogRead(JOY_Y); delay(10); }
  offsetX = (sx/50) - 2048;
  offsetY = (sy/50) - 2048;

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, robotAddr, 6);
  esp_now_add_peer(&peer);
}

void drawSignalBars(int x, int y, int rssi) {
  int bars = 0;
  
  // Se a última mensagem foi há mais de 1.5s, sinal é zero
  if (millis() - lastRecvTime > 1500) {
    bars = 0;
  } else {
    // Escala mais precisa para ESP-NOW
    if (rssi > -50) bars = 4;      // Colado no robô
    else if (rssi > -70) bars = 3; // Perto
    else if (rssi > -85) bars = 2; // Longe (começa a falhar)
    else if (rssi > -98) bars = 1; // No limite do alcance
    else bars = 0;
  }

  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;
    if (i < bars) {
      display.fillRect(x + (i * 4), y + (8 - h), 3, h, WHITE);
    } else {
      display.drawRect(x + (i * 4), y + (8 - h), 3, h, WHITE); // Barra vazia
    }
  }
}

void loop() {
  bool connected = (millis() - lastRecvTime < 2000);

  if (!digitalRead(BTN_MENU)) { 
    currentMenu = (Menu)((currentMenu + 1) % 3); 
    delay(250); 
  }

  // Lógica de Motores e Erros
  if (!digitalRead(JOY_SW)) { 
    tx.motor_count = (tx.motor_count >= 4) ? 0 : tx.motor_count + 2; 
    delay(250); 
  }
  tx.clear_errors = !digitalRead(BTN_CLEAR);

  // Analógicos
  int rawX = analogRead(JOY_X) - offsetX;
  int rawY = analogRead(JOY_Y) - offsetY;
  tx.joyX = map(constrain(rawX, 0, 4095), 0, 4095, -100, 100);
  tx.joyY = map(constrain(rawY, 0, 4095), 0, 4095, -100, 100);
  if(abs(tx.joyX) < 15) tx.joyX = 0; if(abs(tx.joyY) < 15) tx.joyY = 0;

  float pot = analogRead(POT_PIN) / 4095.0;
  if (abs(pot - lastPot) > 0.02) {
    lastPot = pot; tx.speed_limit = pot;
    potTimer = millis(); showSpeedOverlay = true;
  }
  if (showSpeedOverlay && millis() - potTimer > 1200) showSpeedOverlay = false;

  // Failsafe LEDs
  if (!connected || !rx.system_ok) {
    digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, tx.motor_count > 0);
  }

  esp_now_send(robotAddr, (uint8_t *) &tx, sizeof(tx));
  
  display.clearDisplay();
  if (showSpeedOverlay) {
    display.setTextSize(2); display.setCursor(0, 25);
    display.printf("Speed:%d%%", (int)(tx.speed_limit * 100));
  } else {
    // BARRA DE TOPO (FIXA EM TODOS OS MENUS)
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(tx.drive_mode ? "Sport" : "Eco"); // Sport ou Eco
    display.setCursor(55, 0);
    display.print(connected ? "OK" : "NC");
    
    // Nome do Menu Atual no meio
    display.setCursor(70, 0);
    if(currentMenu == MAIN_DASH) display.print("Home");
    else if(currentMenu == TELEMETRY) display.print("Tel");
    else display.print("Logs");

    drawSignalBars(110, 0, rssi_val);
    display.drawFastHLine(0, 10, 128, WHITE);

    switch(currentMenu) {
      case MAIN_DASH:
        display.setCursor(0, 18); display.printf("B1:%.1fV | B2:%.1fV", rx.bat1, rx.bat2);
        display.setCursor(0, 32); display.printf("Speed: %d%%", (int)(tx.speed_limit*100));
        display.setCursor(0, 46); display.printf("Motors: %d", tx.motor_count);
        display.setCursor(0, 56); display.print(tx.clear_errors ? "Clear errors..." : (rx.system_ok ? "System ok" : "Motor in error"));
        break;

      case TELEMETRY:
        display.setCursor(0, 15); display.printf("rpm1:%d | rpm2:%d", rx.rpm1, rx.rpm2);
        display.setCursor(0, 30); display.printf("T1:%.1fC | T2:%.1fC", rx.temp1, rx.temp2);
        display.setCursor(0, 45); display.printf("Vel: %.1f m/s", rx.v_ms);
        break;

      case LOGS_INFO:
        display.setCursor(0, 15); display.printf("rssi: %d dBm", rssi_val);
        display.setCursor(0, 25); display.printf("uptime: %lu s", rx.uptime/1000);
        display.setCursor(0, 35); display.printf("X:%d | Y:%d", tx.joyX, tx.joyY);
        display.setCursor(0, 45); display.print(connected ? "Connection: Ok" : "Connection: Not Ok");
        break;
    }
  }
  display.display();
  delay(20);
}