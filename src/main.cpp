#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

#define BUTTON_1 12

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
IPAddress remote_ip;

void packet_info(int size)
{
  Serial.printf("Received %d bytes ", size);
  Serial.printf("from %s:", udp_server.remoteIP().toString().c_str());
  Serial.print(udp_server.remotePort());
  Serial.println();
}

void packet_host_info()
{
  Serial.print("UDP remote ip:");
  Serial.print(udp_server.remoteIP());
  Serial.println();
  Serial.print("UDP remote port:");
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

void initialize_pins()
{
  pinMode(12, INPUT_PULLUP);  //D6
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(2, OUTPUT);     // Initialize GPIO2 pin as an output
}


void led_blink()
{
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
  delay(100);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(200);
  yield();
  digitalWrite(2, LOW);  // Turn the LED on by making the voltage LOW
  delay(100);            // Wait for a second
  digitalWrite(2, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(200);            // Wait for two seconds
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  initialize_pins();
  create_wifi();

  delay(900);
  udp_server.begin(server_udp_port);
  Serial.printf("UDP port %d\r\n", server_udp_port);

  wifi_info();
}

void start_race()
{
}


String ip_to_String(IPAddress ip)
{
  String s="";

  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);

  return s;
}

void list_clients()
{
  unsigned char softap_stations_cnt;
  struct station_info *stat_info;
  struct ip4_addr *ip_address;
  uint32 uintaddress;

  Serial.println("info start");
  softap_stations_cnt = wifi_softap_get_station_num();
  Serial.println(softap_stations_cnt);

  /*  stat_info = wifi_softap_get_station_info();
  while (stat_info != NULL) {
      ip_address = &stat_info->ip;
      Serial.println(ip4addr_ntoa(ip_address));
      stat_info = STAILQ_NEXT(stat_info, next);
    }

  char station_mac[18] = {0}; sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
  String station_ip = IPAddress((&station_list->ip)->addr).toString();
  */

  for (stat_info = wifi_softap_get_station_info();
       stat_info != NULL;
       stat_info = STAILQ_NEXT(stat_info, next)) {
    ip_address = &stat_info->ip;
    Serial.print("x:");
    Serial.println(ip4addr_ntoa(ip_address));
  }


  Serial.println("info end");
}

void send_reply_packet()
{
  udp_server.beginPacket(udp_server.remoteIP(), 12345);
  itoa(packet_number++, number_str, 10);
  strcpy(replyPacket, "Reply ");
  strcat(replyPacket, number_str);
  udp_server.write(replyPacket);
  udp_server.endPacket();
}

void send_broadcast()
{
  udp_server.beginPacket("192.168.145.255", 12345);
  udp_server.write("start");
  udp_server.endPacket();
//  Serial.println("broadcast sent ...");
}

void loop()
{
  led_blink();

  int switch_status = digitalRead(BUTTON_1);

  if (!switch_status) {
    Serial.println("switch");
  }

  if (stations != WiFi.softAPgetStationNum()) {
    Serial.print("connected clients change:");
    Serial.println(WiFi.softAPgetStationNum());
    stations = WiFi.softAPgetStationNum();

    list_clients();
  }

  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    packet_host_info();

    remote_ip = udp_server.remoteIP();
    String remote_ip_s = remote_ip.toString();

    Serial.println(millis());

    int len = udp_server.read(incomingPacket, 255);

    if (len > 0) {
      incomingPacket[len] = 0;
    }

    Serial.println(incomingPacket);
    Serial.println(remote_ip_s);
    int whereisdot = remote_ip_s.lastIndexOf('.');
    String guest_number = remote_ip_s.substring(whereisdot + 1, whereisdot + 4);
    Serial.print("guest #");
    Serial.println(guest_number);

    send_reply_packet();
    //    Serial.println(millis());

    send_broadcast();

    list_clients();
  }
}
