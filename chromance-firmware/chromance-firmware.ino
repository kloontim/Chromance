/*
   Chromance wall hexagon source (emotion controlled w/ EmotiBit)
   Partially cribbed from the DotStar example
   I smooshed in the ESP32 BasicOTA sketch, too

   (C) Voidstar Lab 2021
*/

#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "mapping.h"
#include "ripple.h"
#include "ripple_rules.h"

const char* ssid = "YourMom";
const char* password = "is a nice lady";

// WiFi stuff - CHANGE FOR YOUR OWN NETWORK!
const IPAddress ip(4, 20, 6, 9);  // IP address that THIS DEVICE should request
const IPAddress gateway(192, 168, 1, 1);  // Your router
const IPAddress subnet(255, 255, 254, 0);  // Your subnet mask (find it from your router's admin panel)

int lengths[] = {154, 168, 84, 154};  // Strips are different lengths because I am a dumb

Adafruit_DotStar strip0(lengths[0], 15, 2, DOTSTAR_BGR);
Adafruit_DotStar strip1(lengths[1], 0, 4, DOTSTAR_BGR);
Adafruit_DotStar strip2(lengths[2], 16, 17, DOTSTAR_BGR);
Adafruit_DotStar strip3(lengths[3], 5, 18, DOTSTAR_BGR);

Adafruit_DotStar strips[4] = {strip0, strip1, strip2, strip3};

byte ledColors[40][14][3];  // LED buffer - each ripple writes to this, then we write this to the strips
int Brigtness = 75;

//Prototype for constnat color: [] () -> unsigned long {return 0xF0F0F0;}
//RuleRipple *ripple = new RuleRipple(&Sauron, 75, 6300, 10, 1, DownFromMiddle);
//SingleRuleRipple *ripple = new SingleRuleRipple(&Sauron, 50, 5000, 10, 15, OldAngry);
SingleRuleRipple *ripples[5] = 
{
  new SingleRuleRipple(Ripple::Const(0xFF0000), Ripple::Const(50), 5000, 10, Ripple::Const(0.9), OldAngry),
  new SingleRuleRipple(Ripple::Const(0x00FF00), Ripple::Const(50), 5000, 10, Ripple::Const(0.9), OldAngry),
  new SingleRuleRipple(Ripple::Const(0x0000FF), Ripple::Const(50), 5000, 10, Ripple::Const(0.9), OldAngry),
  new SingleRuleRipple(Ripple::Const(0xFFFF00), Ripple::Const(50), 5000, 10, Ripple::Const(0.9), OldAngry),
  new SingleRuleRipple(Ripple::Const(0x00FFFF), Ripple::Const(50), 5000, 10, Ripple::Const(0.9), OldAngry),
};

unsigned long Sauron()
{
  unsigned long SomeSauronPallete[10] = 
  {
    0x360405,
    0xF2B01D,
    0x720606,
    0xDA1C09,
    0xEC6810,
    0xA10806,
    0xB80C07,
    0x888484,
    0x546464,
    0xFC4C44
  };

  return SomeSauronPallete[random(0,10)];
}

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

  for(byte i = 0; i < 5; i++)
  {
    ripples[i]->Start(1);
  }
}

void loop() {
  ArduinoOTA.handle();

  // Fade all dots to create trails
  for (int strip = 0; strip < 40; strip++) {
    for (int led = 0; led < 14; led++) {
      for (int i = 0; i < 3; i++) {
        ledColors[strip][led][i] *= 0;//decay;
      }
    }
  }
  
  //if(ripple->IsDead()) ripple->Start(random(0,N_NODES));

  for(byte i = 0; i < 5; i++){
    ripples[i]->Advance();
    ripples[i]->WriteToBuffer(ledColors);  
  }

  //ripple->Advance();
  //ripple->WriteToBuffer(ledColors);

  for (int segment = 0; segment < 40; segment++) {
    for (int fromBottom = 0; fromBottom < 14; fromBottom++) {
      int strip = ledAssignments[segment][0];
      int led = round(fmap(
                        fromBottom,
                        0, 13,
                        ledAssignments[segment][2], ledAssignments[segment][1])); //Map segments to the 4 actual strips
      strips[strip].setPixelColor(
        led,
        ledColors[segment][fromBottom][0],
        ledColors[segment][fromBottom][1],
        ledColors[segment][fromBottom][2]);
    }
  }

  for (int i = 0; i < 4; i++) strips[i].show();
}
