/*
   Chromance wall hexagon source (emotion controlled w/ EmotiBit)
   Partially cribbed from the DotStar example
   I smooshed in the ESP32 BasicOTA sketch, too

   (C) Voidstar Lab 2021
*/

#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <ArduinoOSCWiFi.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "mapping.h"
#include "ripple.h"

const char* ssid = "YourMom";
const char* password = "is a nice lady";

// WiFi stuff - CHANGE FOR YOUR OWN NETWORK!
const IPAddress ip(4, 20, 6, 9);  // IP address that THIS DEVICE should request
const IPAddress gateway(192, 168, 1, 1);  // Your router
const IPAddress subnet(255, 255, 254, 0);  // Your subnet mask (find it from your router's admin panel)
const int recv_port = 42069;  // Port that OSC data should be sent to (pick one, put same one in EmotiBit's OSC Config XML file)

int lengths[] = {154, 168, 84, 154};  // Strips are different lengths because I am a dumb

Adafruit_DotStar strip0(lengths[0], 15, 2, DOTSTAR_BRG);
Adafruit_DotStar strip1(lengths[1], 0, 4, DOTSTAR_BRG);
Adafruit_DotStar strip2(lengths[2], 16, 17, DOTSTAR_BRG);
Adafruit_DotStar strip3(lengths[3], 5, 18, DOTSTAR_BRG);

Adafruit_DotStar strips[4] = {strip0, strip1, strip2, strip3};

byte ledColors[40][14][3];  // LED buffer - each ripple writes to this, then we write this to the strips
float decay = 0.97;  // Multiply all LED's by this amount each tick to create fancy fading tails
int Brigtness = 75;

bool singlePixel = false;

// These ripples are endlessly reused so we don't need to do any memory management
#define numberOfRipples 30
Ripple ripples[numberOfRipples] = {
  Ripple(0),
  Ripple(1),
  Ripple(2),
  Ripple(3),
  Ripple(4),
  Ripple(5),
  Ripple(6),
  Ripple(7),
  Ripple(8),
  Ripple(9),
  Ripple(10),
  Ripple(11),
  Ripple(12),
  Ripple(13),
  Ripple(14),
  Ripple(15),
  Ripple(16),
  Ripple(17),
  Ripple(18),
  Ripple(19),
  Ripple(20),
  Ripple(21),
  Ripple(22),
  Ripple(23),
  Ripple(24),
  Ripple(25),
  Ripple(26),
  Ripple(27),
  Ripple(28),
  Ripple(29),
};

// Pulses
#define randomPulsesEnabled true  // Fire random rainbow pulses from random nodes
#define cubePulsesEnabled true  // Draw cubes at random nodes
#define starburstPulsesEnabled false  // Draw starbursts
#define simulatedBiometricsEnabled false  // Simulate heartbeat and EDA ripples

#define randomPulseTime 2000  // Fire a random pulse every (this many) ms
unsigned long lastRandomPulse;
byte lastAutoPulseNode = 255;

byte numberOfAutoPulseTypes = randomPulsesEnabled + cubePulsesEnabled + starburstPulsesEnabled;
byte currentAutoPulseType = 255;
#define autoPulseChangeTime 30000
unsigned long lastAutoPulseChange;

void setup() {
  Serial.begin(115200);

  Serial.println("*** LET'S GOOOOO ***");

  for (int i = 0; i < 4; i++) {
    strips[i].begin();
        strips[i].setBrightness(Brigtness);  // If your PSU sucks, use this to limit the current
    strips[i].show();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.print("WiFi connected, IP = ");
  Serial.println(WiFi.localIP());

  // Wireless OTA updating? On an ARDUINO?! It's more likely than you think!
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready for WiFi OTA updates");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long benchmark = millis();

  OscWiFi.parse();

  ArduinoOTA.handle();

  // Fade all dots to create trails
  for (int strip = 0; strip < 40; strip++) {
    for (int led = 0; led < 14; led++) {
      for (int i = 0; i < 3; i++) {
        ledColors[strip][led][i] *= decay;
      }
    }
  }

  for (int i = 0; i < numberOfRipples; i++) {
    ripples[i].advance(ledColors);
  }

  for (int segment = 0; segment < 40; segment++) {
    for (int fromBottom = 0; fromBottom < 14; fromBottom++) {
      int strip = ledAssignments[segment][0];
      int led = round(fmap(
                        fromBottom,
                        0, 13,
                        ledAssignments[segment][2], ledAssignments[segment][1]));
      strips[strip].setPixelColor(
        led,
        ledColors[segment][fromBottom][0],
        ledColors[segment][fromBottom][1],
        ledColors[segment][fromBottom][2]);
    }
  }

  for (int i = 0; i < 4; i++) strips[i].show();

  if (numberOfAutoPulseTypes && millis() - lastRandomPulse >= randomPulseTime) {
    unsigned int baseColor = random(0xFFFF);
    singlePixel = !singlePixel;//random(0,1) > 0.5;

    if (currentAutoPulseType == 255 || (numberOfAutoPulseTypes > 1 && millis() - lastAutoPulseChange >= autoPulseChangeTime)) {
      byte possiblePulse = 255;
      while (true) {
        possiblePulse = random(3);

        if (possiblePulse == currentAutoPulseType)
          continue;

        switch (possiblePulse) {
          case 0:
            if (!randomPulsesEnabled)
              continue;
            break;

          case 1:
            if (!cubePulsesEnabled)
              continue;
            break;

          case 2:
            if (!starburstPulsesEnabled)
              continue;
            break;

          default:
            continue;
        }

        currentAutoPulseType = possiblePulse;
        lastAutoPulseChange = millis();
        break;
      }
    }

    switch (currentAutoPulseType) {
      case 0: {
          int node = 0;
          bool foundStartingNode = false;
          while (!foundStartingNode) {
            node = random(25);
            foundStartingNode = true;
            for (int i = 0; i < numberOfBorderNodes; i++) {
              // Don't fire a pulse on one of the outer nodes - it looks boring
              if (node == borderNodes[i])
                foundStartingNode = false;
            }

            if (node == lastAutoPulseNode)
              foundStartingNode = false;
          }

          lastAutoPulseNode = node;

          for (int i = 0; i < 6; i++) {
            if (nodeConnections[node][i] >= 0) {
              for (int j = 0; j < numberOfRipples; j++) {
                if (ripples[j].state == dead) {
                  ripples[j].start(
                    node,
                    i,
//                  strip0.ColorHSV(baseColor + (0xFFFF / 6) * i, 255, 255),
                    strip0.ColorHSV(baseColor, 255, 255),
                    float(random(100)) / 100.0 * .2 + .5,
                    3000,
                    1,
                    singlePixel);

                  break;
                }
              }
            }
          }
          break;
        }

      case 1: {
          int node = cubeNodes[random(numberOfCubeNodes)];

          while (node == lastAutoPulseNode)
            node = cubeNodes[random(numberOfCubeNodes)];

          lastAutoPulseNode = node;

          byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;

          for (int i = 0; i < 6; i++) {
            if (nodeConnections[node][i] >= 0) {
              for (int j = 0; j < numberOfRipples; j++) {
                if (ripples[j].state == dead) {
                  ripples[j].start(
                    node,
                    i,
//                      strip0.ColorHSV(baseColor + (0xFFFF / 6) * i, 255, 255),
                    strip0.ColorHSV(baseColor, 255, 255),
                    .5,
                    2000,
                    behavior,
                    singlePixel);

                  break;
                }
              }
            }
          }
          break;
        }

      case 2: {
          byte behavior = random(2) ? alwaysTurnsLeft : alwaysTurnsRight;

          lastAutoPulseNode = starburstNode;

          for (int i = 0; i < 6; i++) {
            for (int j = 0; j < numberOfRipples; j++) {
              if (ripples[j].state == dead) {
                ripples[j].start(
                  starburstNode,
                  i,
                  strip0.ColorHSV(baseColor + (0xFFFF / 6) * i, 255, 255),
                  .65,
                  1500,
                  behavior,
                  singlePixel);

                break;
              }
            }
          }
          break;
        }

      default:
        break;
    }
    lastRandomPulse = millis();
  }
  //  Serial.print("Benchmark: ");
  //  Serial.println(millis() - benchmark);
}
