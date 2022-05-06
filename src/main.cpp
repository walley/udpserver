#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

#define VERSION "0.1"

#define BUTTON_1 D6 //12
#define BUTTON_2 D7 //13

#define SCREEN_RACE 0xa
#define SCREEN_WIFI 0xb
#define SCREEN_SERVER 0xc
#define SCREEN_LOG 0xd
#define SCREEN_e 0xe
#define SCREEN_ABOUT 0xf

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

int logged_in[8];
char wifi_client_ip[8][20];
unsigned long wifi_client_ping_time[8];
unsigned long wifi_client_ping_rtt[8];

////

char ssid[15]; //"zavod"+channel;
unsigned int server_udp_port = 4210;  // local port to listen on
int stations = 0;
//pocet ocekavanych klientu, podle druhu zavodu
//po pripojeni vsech se spusti faze odpoctu
int stations_max = 2;
LiquidCrystal_PCF8574 lcd(0x27);

////

char incoming_packet[255];  // buffer for incoming packets
char number_str[255];
char replyPacket[50];  // a reply string to send back
bool result;
int packet_number;
IPAddress remote_ip;
int race_state;
int button_2_status;
int button_2_status_last;

int button_1_status;
int switch_status;
int button_1_status_last;
int timer;
int timer_on;
unsigned long racetime;
unsigned long result_left;
unsigned long result_right;
int left_end;
int right_end;
char * time_display;
int lcd_screen = 0xa;
int lcd_current_screen = 0xa;
int first = 0;
int second = 0;

int network_identification;

void lcd_race_state(String msg)
{
  lcd.setCursor(0,0);
  lcd.print("race        !");
  lcd.setCursor(5,0);
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
  network_identification = 2;
  return 140 + network_identification;
}

void set_ipv4()
{
  char chnl[4];

  int x = get_comm_channel();
  snprintf(chnl, 4, "%i", x);
  strcpy(ssid,"zavod");
  strcat(ssid, chnl);
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
  Serial.print("UDP remote:");
  Serial.print(udp_server.remoteIP());
  Serial.print(":");
  Serial.print(udp_server.remotePort());
  Serial.println();
}

void wifi_info()
{
  Serial.printf("MAC address: %s\r\n", WiFi.softAPmacAddress().c_str());
  Serial.print("Soft-AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void initialize_pins()
{
  pinMode(BUTTON_1, INPUT_PULLUP);  //D6,GPIO12
  pinMode(BUTTON_2, INPUT_PULLUP);  //D7,GPIO13

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(D4, OUTPUT);    //D4, GPIO2
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

void lcd_setup()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup: verze "VERSION);
  lcd.setCursor(0, 1);
  lcd.print("wifi  : --");
  lcd.setCursor(0, 2);
  lcd.print("client: --------");
}

void lcd_wifi(char * x)
{
  lcd.setCursor(8, 1);
  lcd.print(x);
}

void lcd_clients(int lane, int state)
{
  Serial.print("lcd_clients:");
  lcd.setCursor(8 + lane, 2);
  lcd.print(state);
  Serial.print(lane);
  Serial.print(",");
  Serial.println(state);
}

void lcd_clients_ping(int client)
{
  if (lcd_screen == SCREEN_WIFI) {
    for (int i = 0; i < 8; i++) {
      lcd.setCursor(i*2, 3);
//      ldiv_t ldivresult;
//      ldivresult = ldiv(wifi_client_ping_rtt[i],10);
//      lcd.printf("%ld",ldivresult.quot);
      lcd.print(wifi_client_ping_rtt[i]/10);
//      Serial.println(wifi_client_ping_rtt[i]);
//      Serial.printf("%ld.\n",ldivresult.quot);
    }
  }
}

void lcd_clients_info()
{
  lcd.home();
  lcd.setCursor(0, 0);
  lcd.printf("st:%i stm:%i.", stations, stations_max);

  for (int i = 0; i < 8; i++) {
    lcd.setCursor(i*2, 2);
    lcd.print(logged_in[i]);
    lcd_clients_ping(i);
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

  for (stat_info = wifi_softap_get_station_info();
       stat_info != NULL;
       stat_info = STAILQ_NEXT(stat_info, next)) {
    ip_address = &stat_info->ip;
    Serial.print("x:");
    Serial.println(ip4addr_ntoa(ip_address));
    //char station_mac[18] = {0}; sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
  }

  Serial.println("info end");
}

bool wifi_create()
{
  lcd_wifi("cr");
  Serial.print("Setting soft-AP ... ");
  WiFi.softAPConfig(*ip_addr, *gateway, *subnet);
  result = WiFi.softAP(ssid, "xxxxxxxxx");

  if (result == true) {
    Serial.println("Ready");
  } else {
    Serial.println("Failed!");
  }

  return result;
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

void send_packet(char * ip, char * p)
{
  udp_server.beginPacket(ip, 12345);
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
  int i;

  for (i = 0; i < 8; i++) {
    if (logged_in[i]) {
      Serial.print("pint to client ");
      Serial.println(i);
      wifi_client_ping_time[i] = millis();
      send_packet(wifi_client_ip[i],"00");
    }
  }

  return 3;
}


int wifi_client_login(int client, char * ip)
{
  logged_in[client] = 1;
  lcd_clients(client, 1);
  strcpy(wifi_client_ip[client], ip);
  Serial.println(ip);
  send_packet(ip, "01");
  stations++;
  Serial.printf("Client %i logged in\n", client);
  return 1;
}

int wifi_wait_for_clients()
{
  int lane;

  stations = 0;

// Serial.println("Waiting for clients ...");

  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    packet_host_info();
    remote_ip = udp_server.remoteIP();
    String remote_ip_s = remote_ip.toString();
    int len = udp_server.read(incoming_packet, 255);
    char remote_ip_c[20];
    remote_ip.toString().toCharArray(remote_ip_c, 19);

    if (len > 0) {
      incoming_packet[len] = 0;
    }

    Serial.print("incoming_packet:");
    Serial.println(incoming_packet);

    lane = incoming_packet[0] - '0';
    Serial.printf("lane:%i\n",lane);

    if (incoming_packet[1] == '0') {
      //login lane
      logged_in[lane] = 1;
      lcd_clients(lane, 1);
      strcpy(wifi_client_ip[lane], remote_ip_c);
      Serial.println(remote_ip_c);
      send_packet(remote_ip_c, "01");
      stations++;
    }

    Serial.println(lane);

  }

  if (stations == stations_max) {
    return 1;
  } else {
    return 0;
  }
}

void lcd_screen_race()
{
  lcd.setCursor(0,3);

  if (timer_on) {
    time_display = millis_to_time(millis() - racetime);
    lcd.print("time:");
    lcd.print(time_display);
    free(time_display);
  } else {
    lcd.print("00:00:00.000");
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
}


void lcd_screen_server()
{
  lcd.setCursor(0,0);
  lcd.print("server:");
  lcd.print(ip_to_String(*ip_addr));

  lcd.setCursor(0,1);
  lcd.print(ip_to_String(*gateway));

  lcd.setCursor(0,2);
  lcd.print(ssid);

  lcd.setCursor(0,3);
  lcd.print(network_identification);
}

void lcd_screen_about()
{
  lcd.setCursor(0,0);
  lcd.print("about udpserver:");
  lcd.setCursor(0,1);
  lcd.print("by michal grezl");
}

void lcd_display_screen()
{

  if (lcd_screen != lcd_current_screen) {
    lcd.clear();
  }

  switch (lcd_screen) {
    case SCREEN_WIFI:
      lcd_clients_info();
      break;

    case SCREEN_RACE:
      lcd_screen_race();
      break;

    case SCREEN_SERVER:
      lcd_screen_server();
      break;

    case SCREEN_LOG:
      break;

    case SCREEN_ABOUT:
      lcd_screen_about();
      break;

      //    case SCREEN_f:
      //      lcd_screen_f();
      //      break;
  }

  lcd_current_screen = lcd_screen;
}

void race_start()
{
  list_clients();
  send_broadcast("02");
  race_state = RACE_STARTED;
  Serial.println("race started");
  initialize_timer();
  lcd_screen = SCREEN_RACE;
  lcd_display_screen();
}

void race_end()
{
  list_clients();
  send_broadcast("03");
  race_state = RACE_ENDED;
  timer_on = 0;
  lcd_screen = SCREEN_RACE;
  lcd_display_screen();
  Serial.println("race ended");
}

void race_abort()
{
  list_clients();
  send_broadcast("04");
  race_state = RACE_ABORTED;
  timer_on = 0;
}

void process_packet()
{
  int lane;
  int client = 0;
  int packetSize = udp_server.parsePacket();

  if (packetSize) {
    if (SCREEN_LOG) {
      lcd.setCursor(19,1);
      lcd.print("#");
    }

    packet_host_info();

    remote_ip = udp_server.remoteIP();
    String remote_ip_s = remote_ip.toString();
    char remote_ip_c[20];
    strcpy(remote_ip_c, remote_ip_s.c_str());

    for (int i = 0; i < 7; i++) {
      if (!strcmp(remote_ip_s.c_str(), wifi_client_ip[i])) {
        client = i;
      }
    }

    Serial.print("client #");
    Serial.println(client);

    int len = udp_server.read(incoming_packet, 255);

    if (len > 0) {
      incoming_packet[len] = 0;
    }

    Serial.print("content:");
    Serial.println(incoming_packet);
    int whereisdot = remote_ip_s.lastIndexOf('.');
    String guest_number = remote_ip_s.substring(whereisdot + 1, whereisdot + 4);
    Serial.print("guest #");
    Serial.println(guest_number);

//////////////
//parse packet
//////////////

//device identification
    lane = incoming_packet[0] - '0';
//message type

// x0 - login
    if (incoming_packet[1] == '0') {
      Serial.printf("Device %i tries to login ...", lane);
      wifi_client_login(lane, remote_ip_c);
    }

// x1 - end race
    if (incoming_packet[1] == '1') {
      //race end
      if (!first && !second) {
        result_left = millis() - racetime;
        time_display = millis_to_time(result_left);
        lcd_lanes(1, time_display);
        left_end = 1;
        first = 1;
        Serial.printf("First client %i, time:%s\n", client, time_display);
        free(time_display);
      } else if (first && !second) {
        result_right = millis() - racetime;
        time_display = millis_to_time(result_left);
        lcd_lanes(2, time_display);
        right_end = 1;
        second = 1;
        Serial.printf("Second client %i, time:%s\n", client, time_display);
        free(time_display);
      }

      if (first && second) {
        race_end();
      }
    }

// x2 pong
    if (incoming_packet[1] == '2') {
      unsigned long ping_time = millis() - wifi_client_ping_time[client];
      Serial.print("pong:");
      Serial.println(ping_time);
      wifi_client_ping_rtt[client] = ping_time;
      // display ping time
      lcd_clients_ping(client);
    }
  }
}

/******************************************************************************/

void setup()
{
  bool wifi;

  for (int i = 0; i< 3; i++) {
    wifi_client_ping_time[i] = 0;
    wifi_client_ping_rtt[i] = 0;
    logged_in[i] = 0;

  }

  Serial.begin(9600);
  Serial.println();
  Serial.println("UDPSERVER verze"VERSION);

  initialize_lcd();
  lcd_setup();

  set_ipv4();
  initialize_pins();
  wifi = wifi_create();

  if (wifi) {
    lcd_wifi(ssid);
  } else {
    lcd_wifi("err");
  }

  delay(900);
  udp_server.begin(server_udp_port);
  Serial.printf("UDP port %d\r\n", server_udp_port);

  wifi_info();
  race_state = RACE_IDLE;
  switch_status = 0;
  timer = 0;
  timer_on = 0;
  left_end = 0;
  right_end = 0;

  //for (; !wifi_wait_for_clients();) {
  //  delay(100);
  //}

  lcd.clear();

}


/*
 *
 *
 */


void loop()
{
  int lane;     //0 none, 1 left, 2 right, 3 third
  char * time_display;
  lcd.setCursor(19,1);
  lcd.print(".");
  unsigned int curent_time = millis();

  if (curent_time % 5 == 0) {
    wifi_ping_clients();
  }

  led_blink();

//handle abortion
  if (race_state == RACE_ABORTED) {
    race_state = RACE_IDLE;
//do something ...
  }

  process_packet();

  button_1_status = digitalRead(BUTTON_1);
  button_2_status = digitalRead(BUTTON_2);

  if (!button_1_status) {
    lcd.setCursor(19,0);
    lcd.print("X");
  } else {
    lcd.setCursor(19,0);
    lcd.print(".");
  }

  if (!button_2_status) {
    lcd.setCursor(19,0);
    lcd.print("Y");
  } else {
    lcd.setCursor(19,0);
    lcd.print(".");
  }

  if (button_1_status != button_1_status_last) {
    if (button_1_status == HIGH && button_1_status_last == LOW) {
      Serial.println("switch press end");

      if (race_state == RACE_IDLE) {
        race_start();
      } else if (race_state == RACE_STARTED) {
        race_abort();  //emergency stop
        delay(1000);
      }
    }

    if (button_1_status == LOW && button_1_status_last == HIGH) {
      Serial.println("switch press start");
    }

  }

  button_1_status_last = button_1_status;

  if (button_2_status != button_2_status_last) {
    if (button_2_status == HIGH && button_2_status_last == LOW) {
      Serial.println("button2 press end");
      lcd_screen++;

      if (lcd_screen > 0xf) {
        lcd_screen = 0xa;
      }

      Serial.println(lcd_screen);
      lcd.println(lcd_screen);
    } else if (race_state == RACE_STARTED) {
    }
  }

  if (button_2_status == LOW && button_2_status_last == HIGH) {
    Serial.println("button2 press start");
  }

  button_2_status_last = button_2_status;

  process_packet();
  lcd_display_screen();
}


