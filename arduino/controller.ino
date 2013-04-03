#include <SPI.h>
#include <Ethernet.h>
#include <XBee.h>
#include <SoftwareSerial.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {  0x90, 0xA2, 0xDA, 0x00, 0xED, 0xF1 };
IPAddress server(196,40,97,162);  // name address for Nedwave API

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;
XBee xbee = XBee();
boolean startRead = false;
String jsonString = "";
unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 60*500;  // delay between updates, in milliseconds
uint8_t d0Cmd[] = { 'D', '0' };
uint8_t d0Value[] = { 0x4 };
uint8_t d0Value2[] = { 0x5 };
uint8_t irCmd[] = {'I','R'};
// Set sample rate to 65 seconds (0xffff/1000)
uint8_t irValue[] = { 0xff, 0xff };

// Connect Arduino pin 8 to TX of usb-serial device
uint8_t ssRX = 8;
// Connect Arduino pin 9 to RX of usb-serial device
uint8_t ssTX = 9;
// Remember to connect all devices to a common Ground: XBee, Arduino and USB-Serial device
SoftwareSerial nss(ssRX, ssTX);

// SH + SL of your remote radio
XBeeAddress64 remoteAddress = XBeeAddress64(0x0013a200, 0x40a4d0b9);
// Create a remote AT request with the IR command
RemoteAtCommandRequest remoteAtRequest = RemoteAtCommandRequest(remoteAddress, irCmd, irValue, sizeof(irValue));
  
// Create a Remote AT response object
RemoteAtCommandResponse remoteAtResponse = RemoteAtCommandResponse();

void setup() {
  Serial.begin(9600);
  nss.begin(9600);
  xbee.begin(Serial);
  Ethernet.begin(mac); 
  delay(9000);
  nss.print("My IP address: ");
  nss.println(Ethernet.localIP());
}

void zoneOn(){
  remoteAtRequest.setCommand(d0Cmd);
  remoteAtRequest.setCommandValue(d0Value);
  remoteAtRequest.setCommandValueLength(sizeof(d0Value));
  sendRemoteAtCommand();
  remoteAtRequest.clearCommandValue();
}

void zoneOff(){
  remoteAtRequest.setCommand(d0Cmd);
  remoteAtRequest.setCommandValue(d0Value2);
  remoteAtRequest.setCommandValueLength(sizeof(d0Value2));
  sendRemoteAtCommand();
  remoteAtRequest.clearCommandValue();
}

void httpRequest() {
  nss.println("Making HTTP Request");
  // if there's a successful connection:
  if (client.connect(server, 80)) {
     nss.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /irrigation/index.php HTTP/1.1");
    client.println("Host: re-source.co.za");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else {
    // if you couldn't make a connection:
    nss.println("connection failed");
    nss.println("disconnecting.");
    client.stop();
  }
}

void loop()
{
    if (client.available()) {
      char c = client.read();
      if( c == '{' ) { startRead = true; }
      if ( startRead ) { jsonString += c; }
    }
    if (!client.connected() && lastConnected) {
       nss.println();
       nss.println("disconnecting.");
       client.stop();
       nss.println("Client disconnected");
       nss.println(jsonString);
       if (getValue()){
        nss.println("turning zone1 on");
        zoneOn();
       } else {
         nss.println("turning zone1 off");
         zoneOff();
       }
       
    }
    if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
      // clear the string
      jsonString = "";
      startRead = false;
      httpRequest();
    }
    // store the state of the connection for next time through
    // the loop:
    lastConnected = client.connected();
}

boolean getValue() {
   int c;
   c = jsonString.indexOf( "zone1");
   if(c){
      Serial.println(jsonString.charAt(c+8));
      if (jsonString.charAt(c+8) == '1'){
        return true;
      } 
   }
   return false;
}

void sendRemoteAtCommand() {
  nss.println("Sending command to the XBee");

  // send the command
  xbee.send(remoteAtRequest);

  // wait up to 5 seconds for the status response
  if (xbee.readPacket(5000)) {
    // got a response!

    // should be an AT command response
    if (xbee.getResponse().getApiId() == REMOTE_AT_COMMAND_RESPONSE) {
      xbee.getResponse().getRemoteAtCommandResponse(remoteAtResponse);

      if (remoteAtResponse.isOk()) {
        nss.print("Command [");
        nss.print(remoteAtResponse.getCommand()[0]);
        nss.print(remoteAtResponse.getCommand()[1]);
        nss.println("] was successful!");

        if (remoteAtResponse.getValueLength() > 0) {
          nss.print("Command value length is ");
          nss.println(remoteAtResponse.getValueLength(), DEC);

          nss.print("Command value: ");
          
          for (int i = 0; i < remoteAtResponse.getValueLength(); i++) {
            nss.print(remoteAtResponse.getValue()[i], HEX);
            nss.print(" ");
          }

          nss.println("");
        }
      } else {
        nss.print("Command returned error code: ");
        nss.println(remoteAtResponse.getStatus(), HEX);
      }
    } else {
      nss.print("Expected Remote AT response but got ");
      nss.print(xbee.getResponse().getApiId(), HEX);
    }    
  } else if (xbee.getResponse().isError()) {
    nss.print("Error reading packet.  Error code: ");  
    nss.println(xbee.getResponse().getErrorCode());
  } else {
    nss.print("No response from radio");  
  }
}

