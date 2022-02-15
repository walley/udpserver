#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

#define BUTTON_1 12

#define RACE_ENDED 0
#define RACE_STARTED 1
#define RACE_ABORTED 2
#define RACE_IDLE 3

#define LEFT_LANE 1
#define RIGHT_LANE 0
#define THIRD_LANE 2

WiFiUDP udp_server;
IPAddress *ip_addr;
IPAddress *gateway;
IPAddress *subnet;

//this should be struct later
char wifi_client_1_ip[20];
char wifi_client_2_ip[20];
char wifi_client_3_ip[20];
int logged_in[3];

////

const char *ssid = "zavod";
unsigned int server_udp_port = 4210;  // local port to listen on
int stations = 0;
int stations_max = 3;
LiquidCrystal_PCF8574 lcd(0x27);

////

char incoming_packet[255];  // buffer for incoming packets
char number_str[255];
char replyPacket[50];  // a reply string to send back
bool result;
int packet_number;
IPAddress remote_ip;
int race_state;
int switch_status;
int switch_status_last;
int timer;
int timer_on;
unsigned long racetime;
unsigned long result_left;
unsigned long result_right;
int left_end;
int right_end;
char * time_display;


void lcd_race_state(String msg)
{
  lcd.setCursor(0,2);
  lcd.print("race           !");
  lcd.setCursor(5,2);
  lcd.print(msg);
}

char * millis_to_time(unsigned long m)
{
  unsigned long runMillis = m;
  unsigned long allSeconds = m / 1000;
  int rest = m % 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;

  char * buf;
  buf = (char *) malloc(24);
  sprintf(buf,"%02d:%02d:%02d.%03d", runHours, runMinutes, runSeconds, rest);

  return buf;
}

int get_comm_channel()
{
  return 145;
}

void set_ipv4()
{
  int x = get_comm_channel();
  ip_addr = new IPAddress(192,168,x,1);  //server ip address
  gateway = new IPAddress(192,168,x,1);
  subnet = new IPAddress(255,255,255,0);
}

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

void wifi_create()
{
  Serial.print("Setting soft-AP ... ");
  WiFi.softAPConfig(*ip_addr, *gateway, *subnet);
  result = WiFi.softAP("zavod", "xxxxxxxxx");

  if (result == true) {
    Serial.println("Ready");
  } else {
    Serial.println("Failed!");
  }

  logged_in[0] = 0;
  logged_in[1] = 0;
  logged_in[2] = 0;
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

void lcd_lanes(int lane, char * msg)
{
  switch (lane) {

    case 0:
      lcd.setCursor(0, 0);
      lcd.print("*** first lane.");
      lcd.setCursor(0, 1);
      lcd.print("*** second lane.");
      break;

    case 1: //left
      lcd.setCursor(0, 0);
      lcd.print(msg);
      break;

    case 2: //right
      lcd.setCursor(0, 1);
      lcd.print(msg);
      break;

    case 3: //third
      lcd.setCursor(0, 2);
      lcd.print(msg);
      break;
  }
}

void lcd_clients(int lane, int state)
{
  Serial.print("lcd_clients:");

  switch (lane) {

    case LEFT_LANE: //left
      lcd.setCursor(18, 0);
      lcd.print(state);
      Serial.print("left");
      Serial.println(state);
      break;

    case RIGHT_LANE: //right
      lcd.setCursor(18, 1);
      lcd.print(state);
      break;

    case THIRD_LANE: //third
      lcd.setCursor(18, 2);
      lcd.print(state);
      break;
  }
}

void led_blink()
{
  return;
  digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on by making the voltage LOW
  delay(100);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(200);
  yield();
  digitalWrite(2, LOW);  // Turn the LED on by making the voltage LOW
  delay(100);            // Wait for a second
  digitalWrite(2, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(200);            // Wait for two seconds
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

void send_packet(char * p)
{
  udp_server.beginPacket("", 12345);
  udp_server.write(p);
  udp_server.endPacket();
}


void heartbeat()
{
  int x = get_comm_channel();

  char client_ip[20];
  sprintf (client_ip, "192.168.%d.1", x);
  //left
  udp_server.beginPacket(client_ip, 12345);
  udp_server.write("18");
  udp_server.endPacket();
//right
  sprintf (client_ip, "192.168.%d.1", x);
  udp_server.beginPacket(client_ip, 12345);
  udp_server.write("08");
  udp_server.endPacket();
}

void send_broadcast(char * msg)
{
  udp_server.beginPacket("192.168.145.255", 12345);
  udp_server.write(msg);
  udp_server.endPacket();
//  Serial.println("broadcast sent ...");
}

void initialize_timer()
{
  timer_on = 1;
  racetime = millis();
}

void race_start()
{
  list_clients();
  send_broadcast("start");
  race_state = RACE_STARTED;
  Serial.println("race started");
  initialize_timer();
  lcd_race_state("started");

}

void race_end()
{
  list_clients();
  send_broadcast("end");
  race_state = RACE_ENDED;
  timer_on = 0;
  lcd_race_state("ended");
}

void race_abort()
{
  list_clients();
  send_broadcast("abort");
  race_state = RACE_ABORTED;
  timer_on = 0;
  lcd_race_state("aborted");
}

void initialize_lcd()
{
  Wire.begin();
  Wire.beginTransmission(0x27);
  int error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error) {
    Serial.println(": LCD not found.");
  }

  Serial.println(": LCD found.");
  lcd.begin(20, 4); // initialize the lcd
  lcd.setBacklight(250);
  lcd.home();
  lcd.clear();
}

int wifi_ping_clients()
{

  return 3;
}

int wifi_wait_for_clients()
{
  int lane;

  Serial.println("Waiting for clients ...");

  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    packet_host_info();
    remote_ip = udp_server.remoteIP();
    String remote_ip_s = remote_ip.toString();
    int len = udp_server.read(incoming_packet, 255);

    if (len > 0) {
      incoming_packet[len] = 0;
    }

    Serial.println(incoming_packet);

    if (incoming_packet[0] == '1') {
      lane = 1;
    } else if (incoming_packet[0] == '0') {
      lane = 2;
    } else {
      lane = 0;
    }

    if (incoming_packet[1] == '0') {
//login lane
      logged_in[lane] = 1;
      lcd_lanes(lane, "nazdar");
      lcd_clients(lane, 1);
      send_packet("01");
    }

    Serial.println(lane);

  }

  /*if (stations != WiFi.softAPgetStationNum()) {
    Serial.print("");
    Serial.println(WiFi.softAPgetStationNum());
    stations = WiFi.softAPgetStationNum();
    list_clients();
  }*/

  if (stations == stations_max) {
    return 1;
  } else {
    return 0;
  }
}

/****************************/

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println("UDPSERVER 0.0");

  set_ipv4();
  initialize_lcd();
  initialize_pins();
  wifi_create();

  delay(900);
  udp_server.begin(server_udp_port);
  Serial.printf("UDP port %d\r\n", server_udp_port);

  wifi_info();
  race_state = RACE_IDLE;
  switch_status = 0;
  timer = 0;
  timer_on = 0;
  lcd_lanes(0,"");
  left_end = 0;
  right_end = 0;

  lcd_clients(1, 0);
  lcd_clients(2, 0);

  for (;;) {
    wifi_wait_for_clients();
    delay(100);
  }
}

void loop()
{
  int lane;     //0 none, 1 left, 2 right, 3 third
  char * time_display;
  lcd.setCursor(19,1);
  lcd.print(".");

  wifi_ping_clients();

  led_blink();

  if (race_state == RACE_IDLE) {
    lcd_race_state("idle");
  }

//handle abortion
  if (race_state == RACE_ABORTED) {
    race_state = RACE_IDLE;
//do something ...
  }

  switch (race_state) {
    case RACE_IDLE:
      lcd_race_state("idle");
      break;

    case RACE_ABORTED:
      lcd_race_state("aborted");
      break;

    case RACE_STARTED:
      lcd_race_state("started");
      break;

    case RACE_ENDED:
      lcd_race_state("ended");
      break;
  }

  switch_status = digitalRead(BUTTON_1);

  if (!switch_status) {
    Serial.println("switch pressed");
    lcd.setCursor(19,0);
    lcd.print("X");
  } else {
    lcd.setCursor(19,0);
    lcd.print(".");
  }

  if (switch_status != switch_status_last) {
    if (switch_status == HIGH && switch_status_last == LOW) {
      Serial.println("switch press end");

      if (race_state == RACE_IDLE) {
        race_start();
      } else if (race_state == RACE_STARTED) {
        race_abort();  //emergency stop
      }
    }


    if (switch_status == 0 && switch_status_last == 1) {
      Serial.println("switch press start");
    }

  }

  switch_status_last = switch_status;

  lcd.setCursor(0,3);

  if (timer_on) {
    time_display = millis_to_time(millis() - racetime);
    lcd.print(time_display);
    free(time_display);
  } else {
    lcd.print("00:00:00.000");
  }

  if (stations != WiFi.softAPgetStationNum()) {
    Serial.print("connected clients change:");
    Serial.println(WiFi.softAPgetStationNum());
    stations = WiFi.softAPgetStationNum();

    list_clients();
  }

  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    lcd.setCursor(19,1);
    lcd.print("#");

    packet_host_info();

    remote_ip = udp_server.remoteIP();
    String remote_ip_s = remote_ip.toString();

    //Serial.println(millis());

    int len = udp_server.read(incoming_packet, 255);

    if (len > 0) {
      incoming_packet[len] = 0;
    }

    Serial.println(incoming_packet);
    Serial.println(remote_ip_s);
    int whereisdot = remote_ip_s.lastIndexOf('.');
    String guest_number = remote_ip_s.substring(whereisdot + 1, whereisdot + 4);
    Serial.print("guest #");
    Serial.println(guest_number);

//parse packet
//which lane side
    if (incoming_packet[0] == '1') {
      lane = 1;
    } else if (incoming_packet[0] == '0') {
      lane = 2;
    } else {
      lane = 0;
    }

//message type
    if (incoming_packet[1] == '0') {
      switch (lane) {
        case 0:
          break;

        case 1:
          Serial.println("Lane 1 relogin ...");
          break;

        case 2:
          break;
      }


    }

    if (incoming_packet[1] == '1') {
      //race end
      if (lane == 1) {
        result_left = millis() - racetime;
        time_display = millis_to_time(result_left);
        lcd_lanes(1, time_display);
        free(time_display);
        left_end = 1;
      } else if (lane == 2) {
        result_right = millis() - racetime;
        time_display = millis_to_time(result_left);
        lcd_lanes(2, time_display);
        free(time_display);
        right_end = 1;
      }

      if (left_end && right_end) {
        race_end();
      }

    } else {
      //something else
    }

    send_reply_packet();
    //    Serial.println(millis());
  }
}

