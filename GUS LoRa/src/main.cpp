#include <WiFi.h>
#include <MQTT.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LoRa.h>
#include <iostream>
#include <string>

// U8X8_SSD1306_128X64_NONAME_4W_SW_SPI u8x8(/* clock=*/ 15, /* data=*/ 4, /* cs=*/ 18, /* dc=*/ 9, /* reset=*/ 16);
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/16, /* clock=*/15, /* data=*/4);

String outgoing; // outgoing message

byte msgCount = 0;        // count of outgoing messages
byte localAddress = 0x42; // address of this device
byte destination = 0x23;  // destination to send to
long lastSendTime = 0;    // last send time
int interval = 2000;
const int csPin = 18;    // LoRa radio chip select
const int resetPin = 14; // LoRa radio reset
const int irqPin = 26;
const char ssid[] = "ThePound";
const char pass[] = "Eeyore2643";
const char mqttaddy[] = "192.168.0.54";

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

void connect()
{
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("arduino", "public", "public"))
  {
    Serial.print(".");
    delay(1000);
  }
  client.subscribe("GUSCommands");
  // client.unsubscribe("/hello");
  Serial.println("\nconnected!");
}

void sendMessage(String outgoing)
{
  Serial.println("Starting LoRa Send " + outgoing);
  LoRa.beginPacket();            // start packet
  LoRa.write(destination);       // add destination address
  LoRa.write(localAddress);      // add sender address
  LoRa.write(msgCount);          // add message ID
  LoRa.write(outgoing.length()); // add payload length
  LoRa.print(outgoing);          // add payload
  LoRa.endPacket();              // finish packet and send it
  msgCount++;                    // increment message ID
}

void onReceive(int packetSize)
{
  if (packetSize == 0)
    return; // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();       // recipient address
  byte sender = LoRa.read();         // sender address
  byte incomingMsgId = LoRa.read();  // incoming msg ID
  byte incomingLength = LoRa.read(); // incoming msg length

  String incoming = "";

  while (LoRa.available())
  {
    incoming += (char)LoRa.read();
  }

  if (incomingLength != incoming.length())
  { // check length for error
    Serial.println("error: message length does not match length");
    return; // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF)
  {
    Serial.println("This message is not for me.");
    return; // skip rest of function
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
  client.publish("GUSPrompt", incoming);
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 6, incoming.c_str());
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);
  sendMessage(payload);
  u8x8.drawString(0, 7, payload.c_str());
  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, pass);
  Serial.println("Beginning mqtt inside setup");
  client.begin(mqttaddy, net);
  Serial.println("Starting callback inside setup");
  client.onMessage(messageReceived);
  Serial.println("Connecting inside setup (wifi?)");
  connect();

  Serial.println("LoRa Duplex");
  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(csPin, resetPin, irqPin); // set CS, reset, IRQ pin

  if (!LoRa.begin(915E6))
  { // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ; // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");

  u8x8.begin();
  u8x8.setPowerSave(0);
}

void loop()
{

  if (!client.connected())
  {
    Serial.println("Connecting to wifi");
    connect();
    delay(10);
  }

  // publish a message roughly every 500 second.
  if (millis() - lastMillis > 500000)
  {
    //Serial.println("Starting mqtt publish");
    lastMillis = millis();
    Serial.println("Publishing...");
    //client.publish("GUSCommands", "Commands mqtt Test from GUS lora");

    Serial.println("Starting LoRa send");
    sendMessage("Yo TTT, c'est moi GUS!");
  }
  // Serial.println("Starting mqtt client.loop main loop");
  //Serial.println("Main loop before onReceive");
  onReceive(LoRa.parsePacket());
  ///Serial.println("Main loop after onReceive before client.loop");
  client.loop();
  //Serial.println("Main loop after client.loop");
  
  //  start lora monitor
  
  delay(10); // <- fixes some issues with WiFi stability

  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 2, "Love you Catherine!");
  //u8x8.setInverseFont(1);
  u8x8.drawString(0, 3, "I'm GUS!");
  //u8x8.setInverseFont(0);
  // u8x8.drawString(0,8,"Line 8");
  // u8x8.drawString(0,9,"Line 9");
  u8x8.refreshDisplay(); // only required for SSD1606/7
  delay(2000);
}