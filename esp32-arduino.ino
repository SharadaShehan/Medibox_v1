#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// Define constants for the OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Define constants for the hardware pins
#define LED 18
#define LDR_LEFT 35
#define LDR_RIGHT 34
#define DHT_PIN 12
#define SERVO_PIN 32
#define BUZZER 19
#define UP_BUTTON 26
#define DOWN_BUTTON 25
#define OK_BUTTON 27
#define CANCEL_BUTTON 33

// Define MQTT topics
#define TEMPERATURE_TOPIC "medibox-210690B-temperature"
#define HUMIDITY_TOPIC "medibox-210690B-humidity"
#define ALARM_ON_TOPIC "medibox-210690B-alarm-on"
#define ALARM_ON_GET_TOPIC "medibox-210690B-alarm-on-get"
#define ALARM_1_TIME_TOPIC "medibox-210690B-alarm-1-time"
#define ALARM_2_TIME_TOPIC "medibox-210690B-alarm-2-time"
#define ALARM_3_TIME_TOPIC "medibox-210690B-alarm-3-time"
#define ALARM_1_TIME_GET_TOPIC "medibox-210690B-alarm-1-time-get"
#define ALARM_2_TIME_GET_TOPIC "medibox-210690B-alarm-2-time-get"
#define ALARM_3_TIME_GET_TOPIC "medibox-210690B-alarm-3-time-get"
#define LIGHT_LEFT_TOPIC "medibox-210690B-light-left"
#define LIGHT_RIGHT_TOPIC "medibox-210690B-light-right"
#define MOTOR_ANGLE_TOPIC "medibox-210690B-motor-angle"

// Create an instance of the DHT sensor
DHTesp dht;

// Create a servo instance
Servo servo;

// Create a wifi client
WiFiClient espClient;
// Create an instance of the MQTT client
PubSubClient mqttClient(espClient);

// Define global variables
int utc_offset = 0;
int dst_offset = 0;
int milliTimenow = 0;
int initmilliseconds = 0;
int timenow[3] = {0, 0, 0};
int buzzer_tones[6] = {262, 294, 330, 349, 392, 440};
int buzzer_tone_count = 6;
float humidity, newHumidity;
float temperature, newTemperature;
int leftLight, rightLight;
int newLeftLight, newRightLight;
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
char temperature_str[6];
char humidity_str[6];
char time_in_millis_str[10];
char light_left_str[6];
char light_right_str[6];

// Create an instance of the OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // Start the serial communication
  Serial.begin(115200);
  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // Initialize the hardware pins
  pinMode(LED, OUTPUT);
  pinMode(LDR_LEFT, INPUT);
  pinMode(LDR_RIGHT, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(UP_BUTTON, INPUT);
  pinMode(DOWN_BUTTON, INPUT);
  pinMode(OK_BUTTON, INPUT);
  pinMode(CANCEL_BUTTON, INPUT);
  pinMode(SERVO_PIN, OUTPUT);

  // Initialize the servo motor
  servo.attach(SERVO_PIN);

  // Initialize the DHT sensor
  dht.setup(DHT_PIN, DHTesp::DHT22);

  display.display();
  delay(2000);

  // Connect to the WiFi network
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    display.println("Connecting to WiFi...");
    display.display();
  }
  display.clearDisplay();
  display.println("Connected to the WiFi network");

  // set mqtt server
  setupMqtt();

  update_time();

  display.display();
  delay(2000);

  display.clearDisplay();

  // Display the welcome message
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Welcome to Medibox");

  display.display();
  delay(2000);

}

void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection....");
    if (mqttClient.connect("ESP-6783-876345267")) {
      Serial.println("connected");
      mqttClient.subscribe(ALARM_ON_GET_TOPIC);
      mqttClient.subscribe(ALARM_1_TIME_GET_TOPIC);
      mqttClient.subscribe(ALARM_2_TIME_GET_TOPIC);
      mqttClient.subscribe(ALARM_3_TIME_GET_TOPIC);
      mqttClient.subscribe(MOTOR_ANGLE_TOPIC);
      // publish alarm status to the MQTT broker
      if (alarms_enabled) {
        mqttClient.publish(ALARM_ON_TOPIC, "true");
      } else {
        mqttClient.publish(ALARM_ON_TOPIC, "false");
      }
      // publish the alarm times to the MQTT broker
      sprintf(time_in_millis_str, "%d", alarm_time_to_millis(alarm_times[0][0], alarm_times[0][1]));
      mqttClient.publish(ALARM_1_TIME_TOPIC, time_in_millis_str);
      sprintf(time_in_millis_str, "%d", alarm_time_to_millis(alarm_times[1][0], alarm_times[1][1]));
      mqttClient.publish(ALARM_2_TIME_TOPIC, time_in_millis_str);
      sprintf(time_in_millis_str, "%d", alarm_time_to_millis(alarm_times[2][0], alarm_times[2][1]));
      mqttClient.publish(ALARM_3_TIME_TOPIC, time_in_millis_str);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

int alarm_time_to_millis(int hour, int minute) {
  return hour * 60 * 60 * 1000 + minute * 60 * 1000;
}

void millis_to_alarm_time(int millis, int alarm_index) {
  alarm_times[alarm_index][0] = (millis / 1000 / 60 / 60) % 24;
  alarm_times[alarm_index][1] = (millis / 1000 / 60) % 60;
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // check if the received message is to enable/disable the alarms
  if (strcmp(topic, ALARM_ON_GET_TOPIC) == 0) {
    // update the alarm status based on the received message
    if (strcmp((char*)payload, "true") == 0) {
      alarms_enabled = true;
    } else {
      alarms_enabled = false;
    }
  }
  // check if the received message is to update the alarm times
  else if (strcmp(topic, ALARM_1_TIME_GET_TOPIC) == 0) {
    millis_to_alarm_time(atoi((char*)payload), 0);
  } else if (strcmp(topic, ALARM_2_TIME_GET_TOPIC) == 0) {
    millis_to_alarm_time(atoi((char*)payload), 1);
  } else if (strcmp(topic, ALARM_3_TIME_GET_TOPIC) == 0) {
    millis_to_alarm_time(atoi((char*)payload), 2);
  }
  // check if the received message is to update the motor angle
  else if (strcmp(topic, MOTOR_ANGLE_TOPIC) == 0) {
    servo.write(atoi((char*)payload));
  }
}

void loop() {
  
  if (!mqttClient.connected()) {
    connectToBroker();
  }

  update_time_and_temp();
  update_light_intensity();

  // Check if the alarm time has been reached and ring the alarm
  if (alarms_enabled) {
    check_alarm_reached();
  }
  // Check if the user wants to go to the menu
  if (digitalRead(OK_BUTTON) == LOW) {
    go_to_menu();
  }

  mqttClient.loop();
  delay(500);
}

// Function to print any text on the OLED display
void print_text_line(String text, int row = 0, int column = 0, int text_size = 1) {
  display.setTextSize(text_size);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

// Function to display the menu
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

// Function to change the time using the NTP server
void update_time() {
  // Connect to the WiFi network
  configTime(utc_offset * 3600, dst_offset * 3600, "pool.ntp.org");
  struct tm timeinfo;
  // Get the current time from the NTP server
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeSecond[3], timeMinute[3], timeHour[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  strftime(timeMinute, 3, "%M", &timeinfo);
  strftime(timeHour, 3, "%H", &timeinfo);
  // Calculate started time in milliseconds, corresponding to the current timezone
  initmilliseconds = atoi(timeSecond) * 1000 + atoi(timeMinute) * 60 * 1000 + atoi(timeHour) * 60 * 60 * 1000 - millis();
  display.clearDisplay();
}

// Function to display the current time zone
void display_time_zone(int utc_offset) {
  display.clearDisplay();
  if (utc_offset >= 0) {
    print_text_line("Current Time Zone: \nUTC+" + String(utc_offset), 0, 0);
  } else {
    print_text_line("Current Time Zone: \nUTC" + String(utc_offset), 0, 0);
  }
}

// Function to set the time zone
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
        print_text_line("Time Zone set to \nUTC+" + String(utc_offset), 0, 0);
      } else {
        print_text_line("Time Zone set to \nUTC" + String(utc_offset), 0, 0);
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

// Function to display the alarm menu
void display_alarm_menu() {
  display.clearDisplay();
  print_text_line("Select alarm to set", 0, 0);
  for (int i = 0; i < alarm_count; i++) {
    if (i == current_alarm_option) {
      // Highlight the current alarm option
      print_text_line("-> Alarm " + String(i + 1) + ": " + String(alarm_times[i][0]) + ":" + String(alarm_times[i][1]), 10 + i * 10, 0);
    } else {
      print_text_line("Alarm " + String(i + 1) + ": " + String(alarm_times[i][0]) + ":" + String(alarm_times[i][1]), 10 + i * 10, 0);
    }
  }
}


// Function to display selected alarm time in hours or minutes
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

// Function to set the alarm time in hours or minutes
bool set_alarm_time_unit(int alarm_index, int time_index) {
  int value = alarm_times[alarm_index][time_index];
  display.clearDisplay();
  // set the alarm time in hours
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
  } 
  // set the alarm time in minutes
  else if (time_index == 1) {
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

// Function to set the alarm time
void set_alarm_time(int alarm_index) {
  display.clearDisplay();
  print_text_line("Set alarm " + String(alarm_index + 1) + " time", 0, 0);
  int prev_hour = alarm_times[alarm_index][0];
  // update alarm time only if setting both hours and minutes is successful
  if (set_alarm_time_unit(alarm_index, 0)) {
    if (set_alarm_time_unit(alarm_index, 1)) {
      display.clearDisplay();
      print_text_line("Alarm " + String(alarm_index + 1) + " time set to " + String(alarm_times[alarm_index][0]) + ":" + String(alarm_times[alarm_index][1]), 0, 0);
      // publish the alarm time to the MQTT broker
      if (!mqttClient.connected()) {
        connectToBroker();
      }
      // copy the alarm time to a string
      sprintf(time_in_millis_str, "%d", alarm_time_to_millis(alarm_times[alarm_index][0], alarm_times[alarm_index][1]));
      if (alarm_index == 0) {
        mqttClient.publish(ALARM_1_TIME_TOPIC, time_in_millis_str);
      } else if (alarm_index == 1) {
        mqttClient.publish(ALARM_2_TIME_TOPIC, time_in_millis_str);
      } else if (alarm_index == 2) {
        mqttClient.publish(ALARM_3_TIME_TOPIC, time_in_millis_str);
      }
      delay(2000);
    } else {
      alarm_times[alarm_index][0] = prev_hour;
    }
  }
  delay(1000);
}

// Function to select alarm option for updating the alarm time
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

// Function to go to the menu
void go_to_menu() {
  show_menu();
  while (true) {
    // Check if the user wants to navigate the menu
    if (digitalRead(UP_BUTTON) == LOW) {
      current_menu_option = (current_menu_option - 1) % menu_option_count;
      if (current_menu_option == -1) { current_menu_option = menu_option_count - 1; }
      show_menu();
    }
    else if (digitalRead(DOWN_BUTTON) == LOW) {
      current_menu_option = (current_menu_option + 1) % menu_option_count;
      show_menu();
    }
    // Navigate to relevant menu option based on user input
    else if (digitalRead(OK_BUTTON) == LOW) {
      if (current_menu_option == 0) {
        set_time_zone();
      } else if (current_menu_option == 1) {
        set_alarm();
      } else if (current_menu_option == 2) {
        // alarms_enabled = !alarms_enabled;
        // publish the alarm status to the MQTT broker
        display.clearDisplay();
        if (!mqttClient.connected()) {
          connectToBroker();
        }
        if (!alarms_enabled) {
          mqttClient.publish(ALARM_ON_TOPIC, "true");
          print_text_line("Alarms enabled", 0, 0);
          alarms_enabled = true;
        } else {
          mqttClient.publish(ALARM_ON_TOPIC, "false");
          print_text_line("Alarms disabled", 0, 0);
          alarms_enabled = false;
        }
        
        // if (alarms_enabled) {
          
        // } else {
          
        // }
        delay(2000);
        show_menu();
      }
    }
    // Return to the main screen
    else if (digitalRead(CANCEL_BUTTON) == LOW) {
      display.clearDisplay();
      return;
    }
  }
}

bool temperatureChanged() {
  if (newTemperature != temperature) {
    temperature = newTemperature;
    return true;
  }
  return false;
}

bool humidityChanged() {
  if (newHumidity != humidity) {
    humidity = newHumidity;
    return true;
  }
  return false;
}

bool leftLightChanged() {
  if (newLeftLight != leftLight) {
    leftLight = newLeftLight;
    return true;
  }
  return false;
}

bool rightLightChanged() {
  if (newRightLight != rightLight) {
    rightLight = newRightLight;
    return true;
  }
  return false;
}

void update_light_intensity() {
  // read the light intensity from the LDR sensors
  newLeftLight = analogRead(LDR_LEFT);
  newRightLight = analogRead(LDR_RIGHT);
  // check if the light intensity has changed
  if (leftLightChanged()) {
    // publish the light intensity to the MQTT broker
    String(leftLight).toCharArray(light_left_str, 6);
    mqttClient.publish(LIGHT_LEFT_TOPIC, light_left_str);
  }
  if (rightLightChanged()) {
    // publish the light intensity to the MQTT broker
    String(rightLight).toCharArray(light_right_str, 6);
    mqttClient.publish(LIGHT_RIGHT_TOPIC, light_right_str);
  }
}

// Function to update the time and temperature on the OLED display
void update_time_and_temp() {
  // read the temperature and humidity from the DHT sensor
  newHumidity = dht.getHumidity();
  newTemperature = dht.getTemperature();

  // check if the humidity has changed
  if (humidityChanged()) {
    String(humidity).toCharArray(humidity_str, 6);
    // publish the humidity to the MQTT broker
    mqttClient.publish(HUMIDITY_TOPIC, humidity_str);
  }

  // check if the temperature has changed
  if (temperatureChanged()) {
    String(temperature).toCharArray(temperature_str, 6);
    // publish the temperature to the MQTT broker
    mqttClient.publish(TEMPERATURE_TOPIC, temperature_str);
  }

  // calculate the current time in milliseconds
  milliTimenow = initmilliseconds + millis();

  // calculate the current time in hours, minutes, and seconds
  timenow[0] = (milliTimenow / 1000 / 60 / 60) % 24;    // hours
  timenow[1] = (milliTimenow / 1000 / 60) % 60;   // minutes
  timenow[2] = (milliTimenow / 1000) % 60;    // seconds

  display.clearDisplay();
  print_text_line("Time: " + String(timenow[0]) + ":" + String(timenow[1]) + ":" + String(timenow[2]), 0, 0);
  print_text_line("Temperature: " + String(temperature) + "C", 10, 0);
  print_text_line("Humidity: " + String(humidity) + "%", 20, 0);
  // Check if the temperature and humidity are within the acceptable range
  // If not, display a warning message and turn on the LED
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

// Function to ring the alarm
void ring_alarm(int alarm_index) {
  digitalWrite(LED, HIGH);
  // keep ringing the alarm until the user stops it
  while (!alarm_ringing_finished[alarm_index]) {
    for (int j = 0; j < buzzer_tone_count; j++) {
      // Check if the user has stopped the alarm
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

// Function to check if the alarm time has been reached
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

