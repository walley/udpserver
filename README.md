# udpserver

esp8266 server comunicates through udp


## protocol

### S -> C

00 - ping
01 - logged in


### C -> S

byte0: lane
byte1: message

--

* x0 -login
* x1 - end
* x2 - pong
* x3 -
