  #include "OneButton.h"
  #include <ESP8266WiFi.h>
  #include <PubSubClient.h>

  #define WARM_LED 12
  #define COLD_LED 16
  #define BUTTON 5

  const char* ssid = "<SSID>";
  const char* password = "<PASSWORD>";
  const char* mqtt_server = "<MQTT_SERVER>";
  const char* mqtt_user = "<MQTT_USER>";
  const char* mqtt_password = "<MQTT_PASSWORD>";

  int warmledState = 0;   // Current set value for WARM leds
  int coldledState = 0;   // Current set value for COLD leds
  int prev_warmledState;
  int prev_coldledState;
  int white_temperature;  // Current white temperature expressed in MIREDS (154<-->500)
  float brightness;
  bool fullstatus;        // Current status of whole strip (on/off) 
  bool automation;        // Whether automatic control is engaged 

  WiFiClientSecure espClient;
  PubSubClient client(espClient);
  OneButton button(BUTTON, true);

  void setup() {
    //Serial.begin(115200);
    delay(10);

    pinMode(WARM_LED, OUTPUT); // WARM_WHITE led
    pinMode(COLD_LED, OUTPUT); // COLD_WHITE led

    setup_wifi();

    client.setServer(mqtt_server, 8883);
    client.setCallback(mqtt_callback);

    button.attachClick(Click);
    button.attachDoubleClick(doubleClick);

    analogWriteRange(255);
  }

  void loop() {
    button.tick();
    if (!client.connected() && WiFi.status() == WL_CONNECTED ) {
      mqtt_reconnect();
    }
    client.loop();
  } 

  void Click() {
    //Serial.println("Single Click");
    // By default fullstatus is inverted on button press
    fullstatus = !fullstatus;
    automation = 0;

    if ( fullstatus ) {
      // Fade both led types UP to the currently set value

      //Serial.print("Calling crossfade with ");
      //Serial.print(warmledState);
      //Serial.print(" and ");
      //Serial.println(coldledState);
      //Serial.println(" target values.");

      crossFade(prev_coldledState, prev_warmledState, coldledState, warmledState);

    } else {
      // Fade both led types DOWN to zero

      //Serial.println("Calling crossfade with ZERO and ZERO target values.");
      crossFade(coldledState, warmledState, 0, 0);
    }

    // Set topic for fullstatus (on/off)
    client.publish("lights/kitchenled/fullstatus",  String(fullstatus).c_str(), true);
    client.publish("lights/kitchenled/automation",  String(automation).c_str(), true);
  }

  void doubleClick() {
    //Serial.println("Double Click - full brightness!");
    //Serial.println("");
    // Doubleclick always brings both led strips to max value
    // regardless of other settings
    warmledState = 255;
    coldledState = 255;
    fullstatus = 1;
    automation = 0;
    white_temperature = 327;
    brightness = 100;
    analogWrite(WARM_LED, warmledState);
    analogWrite(COLD_LED, coldledState);

    // Publish new values
    client.publish("lights/kitchenled/warmstatus",  String(warmledState).c_str(), true);
    client.publish("lights/kitchenled/coldstatus",  String(coldledState).c_str(), true);
    client.publish("lights/kitchenled/fullstatus",  String(fullstatus).c_str(), true);
    client.publish("lights/kitchenled/automation",  String(automation).c_str(), true);
    client.publish("lights/kitchenled/brightness",  String(brightness).c_str(), true);
    client.publish("lights/kitchenled/white_temp",  String(white_temperature).c_str(), true);
  }
   
  void setup_wifi() {

    delay(10);
    // We start by connecting to the WiFi network
    //Serial.println();
    //Serial.print("Connecting to ");
    //Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      //Serial.println();
      //Serial.printf("Connection status: %d\n", WiFi.status());
      //Serial.print(".");
    }

    //Serial.println("");
    //Serial.println("WiFi connected");
    //Serial.println("IP address: ");
    //Serial.println(WiFi.localIP());
  }

  void mqtt_reconnect() {
    if (!client.connected()) {
      //Serial.print("Attempting MQTT connection...");

      String clientId = "MQTT-KitchenLed";
      // Attempt to connect
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
        //Serial.println("connected");
        client.subscribe("lights/kitchenled/warmstatus");
        client.subscribe("lights/kitchenled/coldstatus");
        client.subscribe("lights/kitchenled/fullstatus");
        client.subscribe("lights/kitchenled/white_temp");
        client.subscribe("lights/kitchenled/brightness");
        client.subscribe("lights/kitchenled/automation");
      } else {
        //Serial.print("failed, rc=");
        //Serial.print(client.state());
        //Serial.println(" try again on next loop");
      }
    }
  }

  void mqtt_callback(char* topic, byte* payload, unsigned int length) {

    //Serial.print("Message arrived [");
    //Serial.print(topic);
    //Serial.print("] ");
    //for (int i = 0; i < length; i++) {
      //Serial.print((char)payload[i]);
    //}

  if ( strcmp(topic,"lights/kitchenled/warmstatus") == 0 ) {
    payload[length] = '\0';
    //Serial.print("Getting warmledState: ");
    warmledState = String((char*)payload).toInt();
    //Serial.println(warmledState);
  }
  if ( strcmp(topic,"lights/kitchenled/coldstatus") == 0 ) {
    payload[length] = '\0';
    //Serial.print("Getting coldledState: ");
    coldledState = String((char*)payload).toInt();
    //Serial.println(coldledState);
  } 
  if ( strcmp(topic,"lights/kitchenled/fullstatus") == 0 ) {
    payload[length] = '\0';
    //Serial.print("Getting fullstatus: ");
    fullstatus = String((char*)payload).toInt();
    //Serial.println(fullstatus);
  } 
  if ( strcmp(topic,"lights/kitchenled/white_temp") == 0 ) {
    payload[length] = '\0';
    //Serial.print("Getting temperature: ");
    white_temperature = String((char*)payload).toInt();
    //Serial.println(white_temperature);
  }
  if ( strcmp(topic,"lights/kitchenled/brightness") == 0 ) {
    payload[length] = '\0';
    //Serial.print("Getting brightness: ");
    brightness = String((char*)payload).toInt();
    //Serial.println(brightness);
  }

  if ( fullstatus ) {
    mired2values();
    if ( prev_coldledState != coldledState || prev_warmledState != warmledState) {
      crossFade(prev_coldledState, prev_warmledState, coldledState, warmledState);
    }
  } else {
    if ( prev_coldledState != 0 || prev_warmledState != 0) {
      crossFade(prev_coldledState, prev_warmledState, 0, 0);
    }    
  }

  //Serial.println();
}

void mired2values () {

  // Determine light percentages:
  float warmledState_PC=((white_temperature-154)*100)/173;
  float coldledState_PC=200-warmledState_PC;

  if ( warmledState_PC > 100 ) warmledState_PC = 100;
  if ( coldledState_PC > 100 ) coldledState_PC = 100;

  //Serial.print("Cold white percentage: ");
  //Serial.println(coldledState_PC);
  //Serial.print("Warm white percentage: ");
  //Serial.println(warmledState_PC);

  // Calculate percentage value:
  warmledState = 2.55 * warmledState_PC;
  coldledState = 2.55 * coldledState_PC;

  //  Apply current brightness
  warmledState = warmledState*(brightness/100);
  coldledState = coldledState*(brightness/100);

  //Serial.print("Cold white value: ");
  //Serial.println(coldledState);
  //Serial.print("Warm white value: ");
  //Serial.println(warmledState);
  //Serial.print("Brightness: ");
  //Serial.println(brightness);


}

/* BELOW THIS LINE IS THE MATH -- YOU SHOULDN'T NEED TO CHANGE THIS FOR THE BASICS
* 
* The program works like this:
* Imagine a crossfade that moves the red LED from 0-10, 
*   the green from 0-5, and the blue from 10 to 7, in
*   ten steps.
*   We'd want to count the 10 steps and increase or 
*   decrease color values in evenly stepped increments.
*   Imagine a + indicates raising a value by 1, and a -
*   equals lowering it. Our 10 step fade would look like:
* 
*   1 2 3 4 5 6 7 8 9 10
* R + + + + + + + + + +
* G   +   +   +   +   +
* B     -     -     -
* 
* The red rises from 0 to 10 in ten steps, the green from 
* 0-5 in 5 steps, and the blue falls from 10 to 7 in three steps.
* 
* In the real program, the color percentages are converted to 
* 0-255 values, and there are 1020 steps (255*4).
* 
* To figure out how big a step there should be between one up- or
* down-tick of one of the LED values, we call calculateStep(), 
* which calculates the absolute gap between the start and end values, 
* and then divides that gap by 1020 to determine the size of the step  
* between adjustments in the value.
*/

int calculateStep(int prevValue, int endValue) {
  int step = endValue - prevValue; // What's the overall gap?
  if (step) {                      // If its non-zero, 
    step = 1020/step;              //   divide by 1020
  } 
  return step;
}

/* The next function is calculateVal. When the loop value, i,
*  reaches the step size appropriate for one of the
*  colors, it increases or decreases the value of that color by 1. 
*  (R, G, and B are each calculated separately.)
*/

int calculateVal(int step, int val, int i) {

  if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
    if (step > 0) {              //   increment the value if step is positive...
      val += 1;           
    } 
    else if (step < 0) {         //   ...or decrement it if step is negative
      val -= 1;
    } 
  }
  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255) {
    val = 255;
  } 
  else if (val < 0) {
    val = 0;
  }
  return val;
}

/* crossFade() converts the percentage colors to a 
*  0-255 range, then loops 1020 times, checking to see if  
*  the value needs to be updated each time, then writing
*  the color values to the correct pins.
*/

void crossFade(int cw_start, int ww_start, int cw_end, int ww_end) {
  //Serial.print("Starting crossfade with ");
  //Serial.print(cw_start);
  //Serial.print(" and ");
  //Serial.print(ww_start);
  //Serial.println(" ... ");

  int stepW = calculateStep(ww_start, ww_end);
  int stepC = calculateStep(cw_start, cw_end); 

  int WW_Value = ww_start;
  int CW_Value = cw_start;

  for (int i = 0; i <= 1020; i++) {
    WW_Value = calculateVal(stepW, WW_Value, i);
    CW_Value = calculateVal(stepC, CW_Value, i);

    // Write current values to LED pins
    analogWrite(WARM_LED, WW_Value);
    analogWrite(COLD_LED, CW_Value);

    //delay(crossfadeDelay); // Pause for 'wait' milliseconds before resuming the loop
    delay(1);
  }
  // Update current values for next loop
  prev_warmledState = WW_Value; 
  prev_coldledState = CW_Value;

  //Serial.print(WW_Value);
  //Serial.print(" - ");
  //Serial.println(CW_Value);
  //Serial.println("done.");
}
