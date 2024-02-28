#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define LED 18
#define DHT_PIN 12
#define BUZZER 19
#define UP_BUTTON 26
#define DOWN_BUTTON 25
#define OK_BUTTON 27
#define CANCEL_BUTTON 33

DHTesp dht;

int milliTimenow = 0;
int timenow[3] = {0, 0, 0};
int buzzer_tones[6] = {262, 294, 330, 349, 392, 440};
int buzzer_tone_count = 6;
float humidity;
float temperature;

const int alarm_count = 3;
int alarm_times[alarm_count][2] = {
  {0, 1},
  {1, 0},
  {2, 0}
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  dht.setup(DHT_PIN, DHTesp::DHT22);

  display.display();
  delay(2000);

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello, world!");

  display.display();
  delay(2000);
}

void loop() {
  update_time_and_temp();
  check_alarm_reached();
  delay(1000);
}

void print_text_line(String text, int row = 0, int column = 0, int text_size = 1) {
  display.setTextSize(text_size);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void update_time_and_temp() {
  humidity = dht.getHumidity();
  temperature = dht.getTemperature();
  milliTimenow = millis();
  timenow[0] = milliTimenow / 1000 / 60 / 60;
  timenow[1] = (milliTimenow / 1000 / 60) % 60;
  timenow[2] = (milliTimenow / 1000) % 60;
  display.clearDisplay();
  print_text_line("Time: " + String(timenow[0]) + ":" + String(timenow[1]) + ":" + String(timenow[2]), 0, 0);
  print_text_line("Temperature: " + String(temperature) + "C", 10, 0);
  print_text_line("Humidity: " + String(humidity) + "%", 20, 0);
  display.display();
  delay(1000);
}

void ring_alarm() {
  for (int j = 0; j < buzzer_tone_count; j++) {
    tone(BUZZER, buzzer_tones[j]);
    delay(400);
    noTone(BUZZER);
    delay(2);
  }
}

void check_alarm_reached() {
  for (int i = 0; i < alarm_count; i++) {
    if (alarm_times[i][0] == timenow[0] && alarm_times[i][1] == timenow[1]) {
      display.clearDisplay();
      print_text_line("Alarm time reached!", 0, 0);
      ring_alarm();
    }
  }
}

