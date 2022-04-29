# udpserver

esp8266 server comunicates through udp

## protocol

### S -> C

* 00 - ping
* 01 - logged in
* 02 - race start
* 03 - race end
* 04 - race abort

### C -> S

* byte0: device ID
* byte1: message

#### message  

* x0 - login
* x1 - end
* x2 - pong
* x3 -
