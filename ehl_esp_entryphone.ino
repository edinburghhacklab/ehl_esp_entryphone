// Dependencies:
//   https://github.com/Imroy/pubsubclient
//   https://github.com/thomasfredericks/Bounce2

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>

const char *ssid = "Hacklab";
const char *pass = "piranhas";
const char *mqtt_server = "mqtt";
const char *request_topic = "access/entrance/request";
const char *release_topic = "access/entrance/release";
const unsigned int release_time = 2000;

const int request_pin = 14;
const int release_pin = 4;

char mqtt_id[24];
int last_button_state = 1;
unsigned long last_button_time = 0;
int release_enabled = 0;
unsigned long release_start = 0;

void callback(const MQTT::Publish& pub) {
  if (release_enabled == 0) {
    release_enabled = 1;
    release_start = millis();
    digitalWrite(release_pin, HIGH);
    Serial.println("release enabled");
  } else {
    Serial.println("ignoring redundant release topic");
  }
}

WiFiClient wclient;
PubSubClient client(wclient, mqtt_server);
Bounce debouncer = Bounce();

void setup() {
  pinMode(request_pin, INPUT_PULLUP);
  pinMode(release_pin, OUTPUT);
  
  digitalWrite(release_pin, LOW);
  
  debouncer.attach(request_pin);
  debouncer.update();
  
  Serial.begin(115200);
  Serial.println();

  // generate mqtt id
  snprintf(mqtt_id, sizeof(mqtt_id), "%08x", ESP.getChipId());

  client.set_callback(callback);
}

void loop() {
  if (release_enabled) {
    if ((signed long)(millis() - release_start) > release_time) {
      release_enabled = 0;
      digitalWrite(release_pin, LOW);
      Serial.println("release disabled");
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      return;
    }
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(mqtt_id)) {
        Serial.println("MQTT connected");
        client.subscribe(release_topic);
      }
    }
    if (client.connected()) {
      debouncer.update();
      int new_button_state = debouncer.read();
      if (new_button_state != last_button_state) {
        Serial.print("request pin = ");
        Serial.println(new_button_state, DEC);
        last_button_state = new_button_state;
        if (new_button_state == HIGH) {
          if ((signed long)(millis() - last_button_time) > 500) {
            Serial.println("request received");
            client.publish(request_topic, "");
            last_button_time = millis();
          } else {
            Serial.println("request received (ignored)");
          }
        }
      }
      client.loop();
    }
  }
}

