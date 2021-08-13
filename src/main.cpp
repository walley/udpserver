#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

WiFiUDP udp_server;
IPAddress ip_addr(192,168,145,1);  //server ip address
IPAddress gateway(192,168,145,1);
IPAddress subnet(255,255,255,0);
const char * ssid = "zavod";
unsigned int server_udp_port = 4210;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets
char number_str[255];
char replyPacket[50];  // a reply string to send back
int stations = 0;
bool result;
int packet_number;

void packet_info(int size)
{
  Serial.printf("Received %d bytes ", size);
  Serial.printf("from %s:", udp_server.remoteIP().toString().c_str());
  Serial.print(udp_server.remotePort());
  Serial.println();
}

void create_wifi()
{
  Serial.print("Setting soft-AP ... ");
  WiFi.softAPConfig(ip_addr, gateway, subnet);
  result = WiFi.softAP("zavod", "xxxxxxxxx");

  if (result == true) {
    Serial.println("Ready");
  } else {
    Serial.println("Failed!");
  }
}

void wifi_info()
{
  Serial.printf("MAC address: %s\r\n", WiFi.softAPmacAddress().c_str());
  Serial.print("Soft-AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void packet_info(char * pckt)
{
  Serial.printf("UDP packet contents: %s", pckt);
  Serial.println();
  Serial.print("UDP remote ip:");
  Serial.print(udp_server.remoteIP());
  Serial.println();
  Serial.print("UDP remote port:");
  Serial.print(udp_server.remotePort());
  Serial.println();
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  create_wifi();

  delay(1000);
  udp_server.begin(server_udp_port);
  Serial.printf("UDP port %d\r\n", server_udp_port);

  wifi_info();
}

void send_broadcast()
{
  udp_server.beginPacket("192.168.145.255", 12345);
  udp_server.write("start");
  udp_server.endPacket();
  Serial.println("broadcast sent ...");
}

void loop()
{
  if (stations != WiFi.softAPgetStationNum()) {
    Serial.print("connected clients change:");
    Serial.println(WiFi.softAPgetStationNum());
    stations = WiFi.softAPgetStationNum();
  }

  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    Serial.println(millis());
    packet_info(packetSize);

    int len = udp_server.read(incomingPacket, 255);

    if (len > 0) {
      incomingPacket[len] = 0;
    }

    packet_info(incomingPacket);

    udp_server.beginPacket(udp_server.remoteIP(), 12345);

    itoa(packet_number++, number_str, 10);
    strcpy(replyPacket, "Reply ");
    strcat(replyPacket, number_str);
    udp_server.write(replyPacket);
    udp_server.endPacket();
    Serial.println("Replied ...");
    Serial.println(millis());

    send_broadcast();
  }
}
