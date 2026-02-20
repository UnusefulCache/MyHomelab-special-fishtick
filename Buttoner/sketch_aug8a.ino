#include <WiFi.h>
#include <ESP32Servo.h>
#include <esp_now.h>
Servo servo1;
Servo servo2;
Servo servo3;


//network credentials
const char* ssid = "emcc";
const char* password = "donthackmeiamnoob";

IPAddress local_IP(10, 1, 1, 200);
// Set your Gateway IP address
IPAddress gateway(10, 1, 0, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(10, 1, 0, 1);
IPAddress secondaryDNS(10, 1, 0, 1);
// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;


// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Define Authentication
const char* base64Encoding = "=";  // base64encoding user:pass
bool P7800X3D;
bool P5700X;
bool P3570;
void Task1code( void * parameter) {
  for(;;) {
    delay(100);
  P7800X3D = digitalRead(33);
  P5700X = digitalRead(14);
  P3570 = digitalRead(27);
    if(millis() > 3600000){
      esp_restart();
    }
  }
}
void setup() {
  pinMode(33, INPUT);
  pinMode(14, INPUT);
  pinMode(27, INPUT);
  pinMode(21, OUTPUT);
  servo1.attach(21);
  servo1.write(150);
  pinMode(18, OUTPUT);
  servo2.attach(18);
  servo2.write(150);
  pinMode(19, OUTPUT);
  servo3.attach(19);
  servo3.write(120);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  // Initialize the output variables as outputs
WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      NULL,  /* Task handle. */
      0); /* Core where the task should run */
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // check base64 encode for authentication
            // Finding the right credentials
            if (header.indexOf(base64Encoding)>=0)
            {
            
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              // turns the GPIOs on and off
              if (header.indexOf("GET /PC/on") >= 0) {
                Serial.println("GPIO 21 on");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo1.write(120);
                delay(500);
                servo1.write(150);
              }
              if (header.indexOf("GET /PC/off") >= 0) {
                Serial.println("GPIO 21 off");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo1.write(120);
                delay(1200);
                servo1.write(150);
              }
              if (header.indexOf("GET /amd/on") >= 0) {
                Serial.println("GPIO 19 on");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo2.write(120);
                delay(500);
                servo2.write(150);
              }
              if (header.indexOf("GET /amd/off") >= 0) {
                Serial.println("GPIO 19 on");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo2.write(120);
                delay(1200);
                servo2.write(150);
              }
              if (header.indexOf("GET /intel/on") >= 0) {
                Serial.println("GPIO 18 on");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo3.write(150);
                delay(500);
                servo3.write(120);
              }
              if (header.indexOf("GET /intel/off") >= 0) {
                Serial.println("GPIO 18 off");
                client.println("<meta http-equiv=\"refresh\" content=\"0;url='/'\"/>");
                servo3.write(150);
                delay(1200);
                servo3.write(120);
              }
              
              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              // CSS to style the on/off buttons 
              // Feel free to change the background-color and font-size attributes to fit your preferences
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".text { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px;}");
              client.println(".text2 {background-color: #555555; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px;}</style></head>");
              
              // Web Page Heading
              client.println("<body><h1>ESP32 Web Server</h1>");
              client.println("<p>TURN PC ON</p>");
              client.println("<p><a href=\"/PC/on\"><button class=\"button\">ON</button></a></p>");
              client.println("<p>TRUN PC OFF</p>");
              client.println("<p><a href=\"/PC/off\"><button class=\"button\">OFF</button></a></p>");
              if(!P7800X3D){
                client.println("<p class=\"text\">");
                client.println("ON");
                client.println("</p>");
              }
              if(P7800X3D){
                client.println("<p class=\"text2\">");
                client.println("OFF");
                client.println("</p>");
              }
              // Display current state, and ON/OFF buttons for GPIO 4  
              client.println("<p>TURN AMD 3200G ON</p>");
              // If the output4State is off, it displays the ON button       
              client.println("<p><a href=\"/amd/on\"><button class=\"button\">ON</button></a></p>");
              client.println("<p>TURN AMD 3200G OFF</p>");

              // If the output4State is off, it displays the ON button       
              client.println("<p><a href=\"/amd/off\"><button class=\"button\">OFF</button></a></p>");
              if(!P5700X){
                client.println("<p class=\"text\">");
                client.println("ON");
                client.println("</p>");
              }
              if(P5700X){
                client.println("<p class=\"text2\">");
                client.println("OFF");
                client.println("</p>");
              }
              client.println("<p>TURN I7-3370 ON</p>");
              // If the output15State is off, it displays the ON button       
              client.println("<p><a href=\"/intel/on\"><button class=\"button\">ON</button></a></p>");
              client.println("<p>TURN I7-3370 OFF</p>");
              // If the output15State is off, it displays the ON button       
              client.println("<p><a href=\"/intel/off\"><button class=\"button\">OFF</button></a></p>");
              if(!P3570){
                client.println("<p class=\"text\">");
                client.println("ON");
                client.println("</p>");
              }
              if(P3570){
                client.println("<p class=\"text2\">");
                client.println("OFF");
                client.println("</p>");
              }
              client.println("</body></html>");
              // The HTTP response ends with another blank line
              client.println();
              // Break out of the while loop
              break;
            }
            else{
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Secure\"");
              client.println("Content-Type: text/html");
              client.println();
              client.println("<html>Authentication failed</html>");
            }
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
