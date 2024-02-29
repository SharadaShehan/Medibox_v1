#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

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

int utc_offset = 0;
int dst_offset = 0;

int milliTimenow = 0;
int initmilliseconds = 0;
int timenow[3] = {0, 0, 0};
int buzzer_tones[6] = {262, 294, 330, 349, 392, 440};
int buzzer_tone_count = 6;
float humidity;
float temperature;

int current_alarm_option = 0;
const int alarm_count = 3;
int alarm_times[alarm_count][2] = {
  {0, 1},
  {1, 0},
  {2, 0}
};
bool alarm_ringing_finished[alarm_count] = {false, false, false};
bool alarms_enabled = true;

String menu_options[] = {"Set Time Zone", "Set Alarm", "Enable/Disable Alarms"};
int menu_option_count = 3;
int current_menu_option = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(UP_BUTTON, INPUT);
  pinMode(DOWN_BUTTON, INPUT);
  pinMode(OK_BUTTON, INPUT);
  pinMode(CANCEL_BUTTON, INPUT);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  display.display();
  delay(2000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    display.println("Connecting to WiFi...");
  }

  display.clearDisplay();
  display.println("Connected to the WiFi network");

  update_time();

  display.display();
  delay(2000);

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Welcome to Medibox");

  display.display();
  delay(2000);

}

void loop() {
  update_time_and_temp();
  if (alarms_enabled) {
    check_alarm_reached();
  }
  if (digitalRead(OK_BUTTON) == LOW) {
    go_to_menu();
  }
  delay(500);
}

void print_text_line(String text, int row = 0, int column = 0, int text_size = 1) {
  display.setTextSize(text_size);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void show_menu() {
  display.clearDisplay();
  print_text_line("Menu", 0, 0, 2);
  for (int i = 0; i < menu_option_count; i++) {
    if (i == current_menu_option) {
      print_text_line("-> " + menu_options[i], 20 + i * 10, 0);
    } else {
      print_text_line(menu_options[i], 20 + i * 10, 0);
    }
  }
}

void update_time() {
  configTime(utc_offset * 3600, dst_offset * 3600, "pool.ntp.org");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeSecond[3], timeMinute[3], timeHour[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  strftime(timeMinute, 3, "%M", &timeinfo);
  strftime(timeHour, 3, "%H", &timeinfo);
  // timeinit[0] = atoi(timeHour);
  // timeinit[1] = atoi(timeMinute);
  // timeinit[2] = atoi(timeSecond);
  initmilliseconds = atoi(timeSecond) * 1000 + atoi(timeMinute) * 60 * 1000 + atoi(timeHour) * 60 * 60 * 1000;
  display.clearDisplay();
}

void display_time_zone(int utc_offset) {
  display.clearDisplay();
  if (utc_offset >= 0) {
    print_text_line("Current Time Zone: UTC+" + String(utc_offset), 0, 0);
  } else {
    print_text_line("Current Time Zone: UTC" + String(utc_offset), 0, 0);
  }
}

void set_time_zone() {
  int temp_utc_offset = utc_offset;
  display_time_zone(temp_utc_offset);
  delay(200);
  while (true) {
    if (digitalRead(UP_BUTTON) == LOW) {
      temp_utc_offset++;
      if (temp_utc_offset > 14) {
        temp_utc_offset = -12;
      }
      display_time_zone(temp_utc_offset);
    } else if (digitalRead(DOWN_BUTTON) == LOW) {
      temp_utc_offset--;
      if (temp_utc_offset < -12) {
        temp_utc_offset = 14;
      }
      display_time_zone(temp_utc_offset);
    } else if (digitalRead(OK_BUTTON) == LOW) {
      utc_offset = temp_utc_offset;
      display.clearDisplay();
      if (utc_offset >= 0) {
        print_text_line("Time Zone set to UTC+" + String(utc_offset), 0, 0);
      } else {
        print_text_line("Time Zone set to UTC" + String(utc_offset), 0, 0);
      }
      delay(2000);
      update_time();
      break;
    } else if (digitalRead(CANCEL_BUTTON) == LOW) {
      break;
    }
    delay(200);
  }
  display.clearDisplay();
  show_menu();
  delay(200);
}

void display_alarm_menu() {
  display.clearDisplay();
  print_text_line("Select alarm to set", 0, 0);
  for (int i = 0; i < alarm_count; i++) {
    if (i == current_alarm_option) {
      print_text_line("-> Alarm " + String(i + 1) + ": " + String(alarm_times[i][0]) + ":" + String(alarm_times[i][1]), 10 + i * 10, 0);
    } else {
      print_text_line("Alarm " + String(i + 1) + ": " + String(alarm_times[i][0]) + ":" + String(alarm_times[i][1]), 10 + i * 10, 0);
    }
  }
}

void display_alarm_unit(int time_index, int value) {
  display.clearDisplay();
  if (time_index == 0) {
    print_text_line("Set hour ", 0, 0);
    print_text_line("-> " + String(value), 10, 0);
  } else if (time_index == 1) {
    print_text_line("Set minute ", 0, 0);
    print_text_line("-> " + String(value), 10, 0);
  }
}

bool set_alarm_time_unit(int alarm_index, int time_index) {
  int value = alarm_times[alarm_index][time_index];
  display.clearDisplay();
  if (time_index == 0) {
    display_alarm_unit(time_index, value);
    while (true) {
      if (digitalRead(UP_BUTTON) == LOW) {
        value = (value + 1) % 24;
        display_alarm_unit(time_index, value);
      } else if (digitalRead(DOWN_BUTTON) == LOW) {
        value = (value - 1) % 24;
        if (value == -1) { value = 23; }
        display_alarm_unit(time_index, value);
      } else if (digitalRead(OK_BUTTON) == LOW) {
        alarm_times[alarm_index][time_index] = value;
        return true;
      } else if (digitalRead(CANCEL_BUTTON) == LOW) {
        return false;
      }
      delay(200);
    }
  } else if (time_index == 1) {
    display_alarm_unit(time_index, value);
    while (true) {
      if (digitalRead(UP_BUTTON) == LOW) {
        value = (value + 1) % 60;
        display_alarm_unit(time_index, value);
      } else if (digitalRead(DOWN_BUTTON) == LOW) {
        value = (value - 1) % 60;
        if (value == -1) { value = 59; }
        display_alarm_unit(time_index, value);
      } else if (digitalRead(OK_BUTTON) == LOW) {
        alarm_times[alarm_index][time_index] = value;
        return true;
      } else if (digitalRead(CANCEL_BUTTON) == LOW) {
        return false;
      }
      delay(200);
    }
  }
  return false;
}

void set_alarm_time(int alarm_index) {
  display.clearDisplay();
  print_text_line("Set alarm " + String(alarm_index + 1) + " time", 0, 0);
  int prev_hour = alarm_times[alarm_index][0];
  if (set_alarm_time_unit(alarm_index, 0)) {
    if (set_alarm_time_unit(alarm_index, 1)) {
      display.clearDisplay();
      print_text_line("Alarm " + String(alarm_index + 1) + " time set to " + String(alarm_times[alarm_index][0]) + ":" + String(alarm_times[alarm_index][1]), 0, 0);
      delay(2000);
    } else {
      alarm_times[alarm_index][0] = prev_hour;
    }
  }
  delay(1000);
}

void set_alarm() {
  while (true) {
    if (digitalRead(UP_BUTTON) == LOW) {
      current_alarm_option = (current_alarm_option - 1) % alarm_count;
      if (current_alarm_option == -1) { current_alarm_option = alarm_count - 1; }
      display_alarm_menu();
    } else if (digitalRead(DOWN_BUTTON) == LOW) {
      current_alarm_option = (current_alarm_option + 1) % alarm_count;
      display_alarm_menu();
    } else if (digitalRead(OK_BUTTON) == LOW) {
      set_alarm_time(current_alarm_option);
      display_alarm_menu();
    } else if (digitalRead(CANCEL_BUTTON) == LOW) {
      display.clearDisplay();
      break;
    }
    delay(200);
  } 
}

void go_to_menu() {
  show_menu();
  while (true) {
    if (digitalRead(UP_BUTTON) == LOW) {
      current_menu_option = (current_menu_option - 1) % menu_option_count;
      if (current_menu_option == -1) { current_menu_option = menu_option_count - 1; }
      show_menu();
    } else if (digitalRead(DOWN_BUTTON) == LOW) {
      current_menu_option = (current_menu_option + 1) % menu_option_count;
      show_menu();
    } else if (digitalRead(OK_BUTTON) == LOW) {
      if (current_menu_option == 0) {
        set_time_zone();
      } else if (current_menu_option == 1) {
        set_alarm();
      } else if (current_menu_option == 2) {
        alarms_enabled = !alarms_enabled;
        display.clearDisplay();
        if (alarms_enabled) {
          print_text_line("Alarms enabled", 0, 0);
        } else {
          print_text_line("Alarms disabled", 0, 0);
        }
        delay(2000);
        show_menu();
      }
    } else if (digitalRead(CANCEL_BUTTON) == LOW) {
      display.clearDisplay();
      return;
    }
  }
}

void update_time_and_temp() {
  humidity = dht.getHumidity();
  temperature = dht.getTemperature();
  
  milliTimenow = initmilliseconds + millis();

  timenow[0] = (milliTimenow / 1000 / 60 / 60) % 24;
  timenow[1] = (milliTimenow / 1000 / 60) % 60;
  timenow[2] = (milliTimenow / 1000) % 60;

  display.clearDisplay();
  print_text_line("Time: " + String(timenow[0]) + ":" + String(timenow[1]) + ":" + String(timenow[2]), 0, 0);
  print_text_line("Temperature: " + String(temperature) + "C", 10, 0);
  print_text_line("Humidity: " + String(humidity) + "%", 20, 0);
  if ((temperature > 32 || temperature < 26) && (humidity > 80 || humidity < 60)) {
    print_text_line("Temperature and humidity out of range!", 30, 0);
    display.display();
    digitalWrite(LED, HIGH);
  } else if (temperature > 32 || temperature < 26) {
    print_text_line("Temperature out of range!", 30, 0);
    display.display();
    digitalWrite(LED, HIGH);
  } else if (humidity > 80 || humidity < 60) {
    print_text_line("Humidity out of range!", 30, 0);
    display.display();
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(LED, LOW);
    display.display();
    delay(500);
  }
}

void ring_alarm(int alarm_index) {
  digitalWrite(LED, HIGH);
  while (!alarm_ringing_finished[alarm_index]) {
    for (int j = 0; j < buzzer_tone_count; j++) {
      if (digitalRead(CANCEL_BUTTON) == LOW) {
        alarm_ringing_finished[alarm_index] = true;
        digitalWrite(LED, LOW);
        break;
      }
      tone(BUZZER, buzzer_tones[j]);
      delay(400);
      noTone(BUZZER);
      delay(2);
    }
  }
}

void check_alarm_reached() {
  for (int i = 0; i < alarm_count; i++) {
    if (alarm_times[i][0] == timenow[0] && alarm_times[i][1] == timenow[1]) {
      if (!alarm_ringing_finished[i]) {
        display.clearDisplay();
        print_text_line("Medicine time!", 0, 0, 2);
        ring_alarm(i);
      }
    } else {
      alarm_ringing_finished[i] = false;
    }
  }
}

