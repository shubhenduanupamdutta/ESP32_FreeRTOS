//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> do not modify


#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <Adafruit_MCP23X17.h>
#include <HttpsOTAUpdate.h>
#include <ESP32Servo.h>
#include <BLDC_Motor.h>
#include <WiFiUdp.h>
#include <elapsedMillis.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <UrlEncode.h>


#define MCP1_ADDR 0x20  // all 3 addr pins are open - motherboard
#define MCP2_ADDR 0x27  // all 3 addr pins are shorted - expansion board

#define bc6_voltage_pin 33
uint16_t bc6_voltage_adc_val;
double bc6_low_voltage = 19.0;  // volts (3.3V per cell - 6 cells in series)
double bc6_voltage = 0;

#define NUMPIXELS 28
uint8_t LDR_R_input_pin_array[3] = { 0, 1, 2 };  // 4051   // top, mid, bottom LDR (left)
uint8_t LDR_L_input_pin_array[3] = { 3, 4, 5 };  // 4051   // top, mid, bottom LDR (right)

uint8_t Mux1_ADC_PIN = 35;                     // ESP
uint8_t Mux1_Select_pins[3] = { 27, 14, 13 };  // ESP

uint8_t Mux2_ADC_PIN = 32;                    // esp
uint8_t Mux2_Select_pins[3] = { 2, 12, 15 };  //esp
uint8_t Flap_ldr_pin_array[2] = { 2, 1 };     // mux2 on expansion board  - new bots
// uint8_t Flap_ldr_pin_array[4] = {1, 0, 3, 2  };  // mux2 on expansion board  - B50

uint8_t carrier_LED_pin = 4;  //esp

Adafruit_NeoPixel carrier_led(NUMPIXELS, carrier_LED_pin, NEO_GRB + NEO_KHZ800);
Servo servo_diverter;
WiFiUDP udp;
elapsedMillis pkt_received_time;  // timer to track udp timeout

const char *SSID = "TP-Link_35E3";
const char *PASSWORD = "msort@flexli";
const uint8_t Panel_PB1_input_pin = 7;  // MCP1
bool ledstatus = false;
HttpsOTAStatus_t otastatus;
const uint8_t EMERGENCY_PB_OUTPUT = 14;  //MCP1
const String HTTP_DEBUG_SERVER_URL = "http://192.168.2.109:8080/data";
char *HTTP_OTA_SERVER_URL = "http://192.168.2.109:8080/download";

const char *server_certificate = "";
const uint8_t LED1_output_pin = 1;  // MCP1
const uint8_t LED2_output_pin = 2;  // MCP1

const int UDP_PORT = 9999;
char *UDP_IP = "192.168.2.109";
int udp_request_id = 0;

Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;

String BOT_ID = "";
String CODE_ID = "d127_d133";
String debug_logging_string = "";
TaskHandle_t http_debug_log;
TaskHandle_t diverter_overcurrent_protection_routine_handle;
bool debugger_flag = true;
bool database_logging_flag = false;
HTTPClient http;
HTTPClient http_debugger;
BLDC_Motor bldc;

bool is_infeed = false;
const char GO_LEFT = 'L', GO_RIGHT = 'R', PATH_NOT_FOUND = 'X';
const uint8_t DIV_Relay_input_pin = 3;      // MCP1
const uint8_t DIV_PWM_output_pin = 16;      // ESP (16 on ESP with X1A51B1-v2.2, 33 on ESP with X1A51B1-v2.1 )
const uint8_t DIV_Feedback_input_pin = 34;  // ESP
const uint8_t DIV_CS_Pin = 39;              // ESP
const uint8_t DIV_ACTUATE_RIGHT_ANGLE = 50, DIV_ACTUATE_LEFT_ANGLE = 130;
const uint16_t LIMIT_0_MS = 600, LIMIT_180_MS = 2400;
const uint16_t DIV_CS_LED2_toggle_timer_threshold = 500;
const uint8_t DIV_CS_moving_avg_window = 10;
const uint8_t DIV_CS_routine_delay = 10;         // delay in ms between reading current values
const float DIV_CS_sensitivity = 0.4;            // V/A
const float DIV_CS_Vref = 2.46;                  // curent sensor output voltage with no current: ~ 2500mV or 2.5V
const float DIV_CS_avg_current_threshold = 1.5;  // A
elapsedMillis DIV_CS_LED2_toggle_timer = 0;
uint16_t DIV_CS_adcValue = 0;
float DIV_CS_pin_voltage, DIV_CS_ACS723_output_voltage, DIV_CS_insta_current, DIV_CS_avg_current;
bool DIV_FLAG = true;
// const int DIV_TIMEOUT_LIMIT = 700;
// const uint8_t DIV_ACTUATE_BUFFER_ANGLE = 10;
// const int DIV_FEEDBACK_LOWER_LIMIT = 469;
// const int DIV_FEEDBACK_UPPER_LIMIT = 2667;



const int DIV_TIMEOUT_LIMIT = 700;
const uint8_t DIV_CS_MOVING_AVG_WINDOW = 25;
const uint8_t DIV_ACTUATE_BUFFER_ANGLE = 10;
const uint8_t Panel_PB2_input_pin = 6;  // MCP1


String data = "";

// this function takes in debug logs and concatenate with previous log and will be set to send Http request to debugger
void add_log(String log) {
  if (debugger_flag == true) {
    debug_logging_string = debug_logging_string + " " + log;
    debug_logging_string += " | ";
    Serial.println(log);
  } else {
    Serial.println(log);
  }
}

void database_logging() {
  String logurl = "http://192.168.2.109/m-sort-distribution-server/LogItForESP?botId=" + BOT_ID + "&log=" + urlEncode(debug_logging_string);  // confirm if the api takes 0-3 or 1-4 as compartmen id
  http.begin(logurl);
  int loghttpCode = http.PUT(logurl);
  bool log_flag = false;
  if (loghttpCode == HTTP_CODE_OK) {
    add_log("Logging Successful!");
  } else {
    add_log("Logging Failed : " + String(loghttpCode));
  }
}

// Core 2 finction which is sending http post request to debugger with debug logs as payload
void HTTP_DEBUG_LOGGER(void *pvParameters) {
  while (true) {
    // if (publish_debug_log_flag) {
    // publish_debug_log_flag = false;
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    bc6_voltage_adc_val = analogRead(bc6_voltage_pin);
    bc6_voltage = ((3.3 * bc6_voltage_adc_val) / 4096) * 11;
    if (bc6_voltage < bc6_low_voltage) {
      add_log("low_voltage");

    } else {
      add_log(String(bc6_voltage) + "V");
    }
    http_debugger.begin(HTTP_DEBUG_SERVER_URL);
    int httpCode = -1;
    httpCode = http_debugger.POST(debug_logging_string);
    // add_log(String(httpCode));

    if (database_logging_flag) {
      database_logging();
    }

    if (httpCode == HTTP_CODE_OK) {
      debug_logging_string = BOT_ID + " " + CODE_ID + " ";
      //String payload = http_debugger.getString();
    }
  }
}

// need to be ported to class variables

void DIVERTER_OVERCURRENT_PROTECTION_ROUTINE(void *pvParameters) {
  while (true) {
    vTaskDelay(DIV_CS_routine_delay / portTICK_PERIOD_MS);
    DIV_CS_adcValue = analogRead(DIV_CS_Pin);
    DIV_CS_pin_voltage = 3.3 * ((DIV_CS_adcValue * 1.0) / 3800);
    DIV_CS_ACS723_output_voltage = 1.5 * DIV_CS_pin_voltage;  // 1.5 factor --> voltage divider
    DIV_CS_insta_current = (DIV_CS_ACS723_output_voltage - DIV_CS_Vref) / DIV_CS_sensitivity;
    DIV_CS_avg_current = (DIV_CS_avg_current * (DIV_CS_moving_avg_window - 1) + DIV_CS_insta_current) / DIV_CS_moving_avg_window;
    if (DIV_CS_avg_current > DIV_CS_avg_current_threshold) {
      mcp1.digitalWrite(DIV_Relay_input_pin, 1);
      bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
      add_log("DIVERTER STALL DETECTED. Bot Stopped. LED2 Blinking. ");
      DIV_FLAG = false;
      bool led_state = 0;
      while (mcp1.digitalRead(Panel_PB2_input_pin))  // until PB2 is not pushed, keep blinking LED2
      {
        if (DIV_CS_LED2_toggle_timer > DIV_CS_LED2_toggle_timer_threshold) {
          DIV_CS_LED2_toggle_timer = 0;
          led_state = !(led_state);
          mcp1.digitalWrite(LED2_output_pin, led_state);
        }
      }
      DIV_FLAG = true;
      bldc.actuate(BLDC_Motor::_GO_FORWARD, BLDC_Motor::_PWM_S2);
      add_log("DIVERTER STALL resolved. Bot Started. ");
    }
  }
}

int _LDR_current_value_array[6] = { 0, 0, 0, 0, 0, 0 };
int _LDR_previous_value_array[6] = { 0, 0, 0, 0, 0, 0 };
int _LDR_previous_state_array[6] = { 0, 0, 0, 0, 0, 0 };
int _LDR_current_state_array[6] = { 0, 0, 0, 0, 0, 0 };


int _LDR_start_time_array[6] = { 0, 0, 0, 0, 0, 0 };
int _LDR_state_duration_array[6] = { 0, 0, 0, 0, 0, 0 };

//uint8_t segment_flap_ls[1] = { 8 };    //mcp2

uint8_t segment_flap_emg[2] = { 14, 15 };  //mcp2  - new bots

//uint8_t segment_sensor[1] = { 1 };  //mcp2
// uint8_t crash_button = 3;           //mcp2

String _inst_column_code_array[2] = { "000", "000" };
// // String prev_inst_column_code[]={"000","000"};
// //////////////////////////////////////

const int LDR_UPPER_THR_LIMIT = 300;
const int LDR_LOWER_THR_LIMIT = 110;

const int FLAP_LDR_ADC_UPPER_LIMIT = 600;  //change values
const int FLAP_LDR_ADC_LOWER_LIMIT = 100;  //change values

uint8_t _LED_detection_flag[6] = { 0, 0, 0, 0, 0, 0 };
int _LED_detection_timer[6] = { 0, 0, 0, 0, 0, 0 };
String intmt_column_code[2] = { "000", "000" };
String prev_intmt_column_code[2] = { "000", "000" };
String final_column_code[2] = { "000", "000" };
String final_column_code_array[6] = { "000", "000", "000", "000", "000", "000" };





int flaps_ldr_raw_values[1] = { 0 };
bool flaps_ldr_state_values[1] = { 0 };

const static int N_STATIONS = 34;
const String INFEED_LIST[2] = { "I1", "I2" };
const String STATION_ID_LIST[N_STATIONS] = { "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10",
                                             "D11", "D12", "D13", "D14", "D15", "D16", "D17", "D18", "D19", "D20",
                                             //  "D21", "D22", "D23", "D24",
                                             //  "D25", "D26", "D27", "D28", "D29", "D30", "D31", "D32", "D33", "D34", "D35", "D36",
                                             //  "D37", "D38", "D39", "D40", "D41", "D42", "D43", "D44", "D45", "D46", "D47", "D48",
                                             //  "S11", "S12", "S14", "S15", "S17",
                                             //  "S21", "S22", "S23", "S24", "S25", "S26", "S27",
                                             //  "S31", "S32", "S33", "S34", "S35", "S36", "S37",
                                             //  "S41", "S42", "S44", "S45", "S47",
                                             "I11", "I12", "I13", "I1",
                                             "I21", "I22", "I23", "I2",
                                             //  "I31", "I32", "I33", "I3",
                                             //  "I41", "I42", "I43", "I4",
                                             "C1", "C2", "C5", "C9", "C13", "C14" };

const String LEFT_STATION_ID_LIST[N_STATIONS] = {
  "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10", "I21",
  "D12", "D13", "D14", "D15", "D16", "D17", "D18", "D19", "D20", "I11",
  "", "", "", "D1",
  "", "", "", "D11",
  "", "", "", "", "", ""
  // "D2", "D3", "D4", "D5", "D6", "S14", "D8", "D9", "D10", "D11", "D12", "I11",
  // "D14", "D15", "D16", "D17", "D18", "I21", "D20", "D21", "D22", "D23", "D24", "S47",
  // "D26", "D27", "D28", "D29", "D30", "I41", "D32", "D33", "D34", "D35", "D36", "S44",
  // "D38", "D39", "D40", "D41", "D42", "I31", "D44", "D45", "D46", "D47", "D48", "S17",
  // "D7", "", "D37", "", "",
  // "", "", "", "C6", "", "", "",
  // "", "", "", "C7", "", "", "",
  // "D25", "", "D13", "", "",
  // "", "", "", "D1",
  // "", "", "", "D19",
  // "", "", "", "D43",
  // "", "", "", "D31",
  // "", "C8", "C1", "S34", "S24", "C12", "C5", "", "", "", "", ""
};

const String RIGHT_STATION_ID_LIST[N_STATIONS] = {
  "", "", "", "", "", "C5", "", "C9", "", "C14",
  "", "", "", "", "", "C2", "", "C1", "", "C13",
  //  "D21", "D22", "D23", "D24",
  //  "D25", "D26", "D27", "D28", "D29", "D30", "D31", "D32", "D33", "D34", "D35", "D36",
  //  "D37", "D38", "D39", "D40", "D41", "D42", "D43", "D44", "D45", "D46", "D47", "D48",
  //  "S11", "S12", "S14", "S15", "S17",
  //  "S21", "S22", "S23", "S24", "S25", "S26", "S27",
  //  "S31", "S32", "S33", "S34", "S35", "S36", "S37",
  //  "S41", "S42", "S44", "S45", "S47",
  "I12", "I13", "I1", "",
  "I22", "I23", "I2", "",
  //  "I31", "I32", "I33", "I3",
  //  "I41", "I42", "I43", "I4",
  "D3", "D5", "D15", "D13", "D1", "D11"
  //  "", "", "", "", "", "", "", "", "", "", "", "C13",
  //  "", "", "", "", "", "C14", "", "", "", "", "", "",
  //  "", "", "", "", "", "C16", "", "", "", "", "", "",
  //  "", "", "", "", "", "C15", "", "", "", "", "", "",
  //  "S12", "", "S15", "", "C4",
  //  "", "", "", "S25", "", "", "",
  //  "", "", "", "S35", "", "", "",
  //  "S42", "", "S45", "", "C9",
  //  "I12", "I13", "I1", "",
  //  "I22", "I23", "I2", "",
  //  "I32", "I33", "I3", "",
  //  "I42", "I43", "I4", "",
  //  "S11", "S21", "", "", "", "", "S31", "S41", "D1", "D19", "D43", "D31"
};

const String STATION_CODE_LIST[N_STATIONS] = { "001001001", "001001010", "001001011", "001001100", "001001101", "001001110", "001010001", "001010010", "001010011", "001010100",
                                               "001010101", "001010110", "001011001", "001011010", "001011011", "001011100", "001011101", "001011110", "001100001", "001100010",
                                               //  "001100011", "001100100", "001100101", "001100110",
                                               //  "001101001", "001101010", "001101011", "001101100", "001101101", "001101110", "001110001", "001110010", "101001011", "001110100", "001110101", "001110110",
                                               //  "010001001", "010001010", "010001011", "010001100", "010001101", "010001110", "010010001", "010010010", "010010011", "010010100", "010010101", "010010110",
                                               //  "011001001", "011001010", "011001100", "011001101", "011010001",
                                               //  "011010011", "011010100", "011010101", "011010110", "011011001", "011011010", "011011011",
                                               //  "011011101", "011011110", "011100001", "011100010", "011100011", "011100100", "011100101",
                                               //  "011101001", "011101010", "011101100", "011101101", "011110001",
                                               "100001001", "100001010", "100001011", "100001101",
                                               "100001110", "100010001", "100010010", "100010100",
                                               //  "100010101", "100010110", "100011001", "100011011",
                                               //  "100011100", "100011101", "100011110", "100100010",
                                               "110001001", "110001010", "110001101", "110010011", "110011001", "110011010" };

String dropoff_station_list[1] = { "X" };
bool dropoff_motherbag_state[1] = { 0 };
bool dropoff_state[1] = { 0 };


const static char _GO_LEFT = 'L', _GO_RIGHT = 'R', _DISABLE = 'X', _GO_CENTER = 'C', _GO_0 = '0', _GO_180 = '1';
String plannedPath[100];
int station_count = 0;
int planned_path_length;
String drop_station_id = "X";  //initally set to X to indicate that no dropping station is assigned
int _LDR_time_array[6] = { 0, 0, 0, 0, 0, 0 };
String station_code[2] = { "", "" };
String exceptedStation = "";
String currentStation = "";
String currentInfeed = "I1";
bool localization_flag = true;


const int DIV_FEEDBACK_LOWER_LIMIT = 469;
const int DIV_FEEDBACK_UPPER_LIMIT = 2667;

float _loop_time = 0;
float _loop_start_time = 0;
float _max_loop_time = 0;
bool _udp_looptime_flag = false;
int _loop_count = 0;
bool dropoff_update_flag = false;  // false initally, becomes true only on successfull dropoff


// this function is used by mux to change the select pin to access individual ldrs
void set_select_pin(byte pin, uint8_t muxId) {
  for (int i = 0; i < 3; i++) {
    if (pin & (1 << i)) {
      if (muxId == 1) {
        digitalWrite(Mux1_Select_pins[i], HIGH);
      } else if (muxId == 2) {
        digitalWrite(Mux2_Select_pins[i], HIGH);
      }
    } else {
      if (muxId == 1) {
        digitalWrite(Mux1_Select_pins[i], LOW);
      } else if (muxId == 2) {
        digitalWrite(Mux2_Select_pins[i], LOW);
      }
    }
  }
}


void setup() {

  Serial.begin(115200);
  while (!Serial)
    ;
  // initalizing EEPROM and reseting ESP if it fails
  if (!EEPROM.begin(1000)) {
    add_log("Failed to initialise EEPROM");
    add_log("ESP32 Restarting...");
    delay(1000);
    ESP.restart();
  } else {
    //add_log("EEPROM working fine");
    BOT_ID = EEPROM.readString(20);
    // BOT_ID = "B1";
  }
  add_log("BOT ID = " + BOT_ID + "Firmware: " + String(__FILE__));

  while (!mcp1.begin_I2C(MCP1_ADDR)) {
    add_log("MCP1 Error.");
    delay(1000);
  }
  while (!mcp2.begin_I2C(MCP2_ADDR)) {
    add_log("MCP2 Error.");
    delay(1000);
  }

  pinMode(Mux1_ADC_PIN, INPUT);
  pinMode(Mux2_ADC_PIN, INPUT);
  for (int i = 0; i < 3; i++) {
    pinMode(Mux1_Select_pins[i], OUTPUT);
    digitalWrite(Mux1_Select_pins[i], HIGH);
    pinMode(Mux2_Select_pins[i], OUTPUT);
    digitalWrite(Mux2_Select_pins[i], HIGH);
  }
  mcp1.pinMode(EMERGENCY_PB_OUTPUT, OUTPUT);
  mcp1.pinMode(DIV_Relay_input_pin, OUTPUT);
  mcp1.digitalWrite(EMERGENCY_PB_OUTPUT, 0);

  mcp1.pinMode(LED1_output_pin, OUTPUT);
  mcp1.pinMode(LED2_output_pin, OUTPUT);
  mcp1.pinMode(Panel_PB1_input_pin, INPUT);
  mcp1.pinMode(Panel_PB2_input_pin, INPUT);

  pinMode(bc6_voltage_pin, INPUT);

  add_log("Connecting... " + String(SSID) + " " + String(PASSWORD));
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    add_log(".");
    delay(500);
  }
  //ack_led.actuate(Relay::_ON);
  add_log(" Wifi Connected! : ");
  // creating a core 2 task to handle debugger http request
  xTaskCreatePinnedToCore(
    HTTP_DEBUG_LOGGER,
    "debug_logging",
    10000,
    NULL,
    1,
    &http_debug_log,
    1);
  //  creating a core 2 task to check for diverter anti-stall
  xTaskCreatePinnedToCore(
    DIVERTER_OVERCURRENT_PROTECTION_ROUTINE,
    "DIVERTER_OVERCURRENT_PROTECTION_ROUTINE",
    10000,
    NULL,
    1,
    &diverter_overcurrent_protection_routine_handle,
    1);

  // initialize diverter
  pinMode(DIV_CS_Pin, INPUT);
  mcp1.pinMode(DIV_Relay_input_pin, OUTPUT);
  mcp1.digitalWrite(DIV_Relay_input_pin, 0);
  servo_diverter.attach(DIV_PWM_output_pin, LIMIT_0_MS, LIMIT_180_MS);
  servo_diverter.attach(16, LIMIT_0_MS, LIMIT_180_MS);
  udp.begin(UDP_PORT);


  add_log("loop starting");
  pinMode(Mux1_ADC_PIN, INPUT);
  pinMode(Mux2_ADC_PIN, INPUT);
  mcp1.pinMode(LED1_output_pin, OUTPUT);
  mcp1.pinMode(LED2_output_pin, OUTPUT);

  for (int i = 0; i < 3; i++) {
    pinMode(Mux1_Select_pins[i], OUTPUT);
    digitalWrite(Mux1_Select_pins[i], HIGH);
    pinMode(Mux2_Select_pins[i], OUTPUT);
    digitalWrite(Mux2_Select_pins[i], HIGH);
  }
  mcp1.pinMode(Panel_PB1_input_pin, INPUT);

  mcp1.digitalWrite(LED1_output_pin, 1);
  // this condition will set the esp to take OTA update
  if (!mcp1.digitalRead(Panel_PB1_input_pin)) {  //0.4ms <--looptime
    digitalWrite(26, 0);
    mcp1.digitalWrite(LED1_output_pin, 0);

    add_log("ota triggered via button press");
    update_firmware();
  }
  carrier_led.begin();
  carrier_led.setBrightness(255);

  for (int i = 0; i < 2; i++) {
    mcp2.pinMode(segment_flap_emg[i], OUTPUT);
    mcp2.digitalWrite(segment_flap_emg[i], 0);  // turned on the electromagnet just at the automatic flap closure mechanism
  }
  for (int i = 0; i <= 27; i++) {
    carrier_led.setPixelColor(i, carrier_led.Color(0, 0, 0));
  }
  carrier_led.show();
  add_log("Checking diverter : ");

  //Check the diverter
  //First go left
  servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
  delay(500);
  //Next go right
  servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
  delay(500);
  //Finally comeback to left
  servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
  delay(500);

  mcp1.digitalWrite(LED2_output_pin, 0);
  while (mcp1.digitalRead(Panel_PB2_input_pin)) {
    ;
  }
  mcp1.digitalWrite(LED2_output_pin, 1);

  bldc.init(mcp1, 26, BLDC_Motor::_BRAKE_ON, 15, BLDC_Motor::_DIR_CCW, 25, 17, BLDC_Motor::_PWM_S0);
  if (DIV_FLAG == true) {
    bldc.actuate(BLDC_Motor::_GO_FORWARD, BLDC_Motor::_PWM_S2);  // initally motor will be set to move forward
  }


  pinMode(bc6_voltage_pin, INPUT);
  
}


void loop() {

  // battery voltage sensing


  _loop_start_time = millis();
  _udp_looptime_flag = false;


  bool left_station_detected = false;
  bool right_station_detected = false;

  // reading the ldr raw values and based on the thresholds led states will be set
  for (int i = 0; i < 6; i++) {
    set_select_pin(i, 1);  //each ldr pin will be itterated
    _LDR_previous_state_array[i] = _LDR_current_state_array[i];

    // switch case was used to get the values labels and values of ldr, but now this switch case can be removed and simple equating can be made
    switch (i) {
      case 0:
        data = "LDR-LEFT-TOP";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);

        break;

      case 1:
        data = "LDR-LEFT-MID";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);
        break;

      case 2:
        data = "LDR-LEFT-BTM";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);
        break;

      case 3:
        data = "LDR-RIGHT-TOP";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);
        break;

      case 4:
        data = "LDR-RIGHT-MID";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);
        break;

      case 5:
        data = "LDR-RIGHT-BTM";
        _LDR_current_value_array[i] = map(analogRead(Mux1_ADC_PIN), 0, 4096, 0, 1024);
        break;
      default:
        break;
    }
    // checkinhg if ldr value has crossed the upper limit to change to state to 1
    if (_LDR_current_value_array[i] > LDR_UPPER_THR_LIMIT) {
      _LDR_current_state_array[i] = 1;
    }
    // checking the ldr value has crossed below lower limit to change the state to 0
    else if (_LDR_current_value_array[i] < LDR_LOWER_THR_LIMIT) {
      _LDR_current_state_array[i] = 0;
    }

    // comparing if the state has changed from previous state and record the duration of the previous state. this condition is not useful as of now, but will be useful for station assumption
    if (_LDR_previous_state_array[i] != _LDR_current_state_array[i]) {
      if (_LED_detection_flag[i] != 1 && (_LDR_previous_state_array[i] == 0 && _LDR_current_state_array[i] != 0)) {
        _LED_detection_timer[i] = millis();
        _LED_detection_flag[i] = 1;
      }

      _LED_detection_flag[i] = 0;
    }
    _LDR_state_duration_array[i] = millis() - _LDR_start_time_array[i];
    _LDR_start_time_array[i] = millis();
    // //add_log("LED detection count = " + String(_LED_detection_count));
  }


  _inst_column_code_array[0] = String(_LDR_current_state_array[0]) + String(_LDR_current_state_array[1]) + String(_LDR_current_state_array[2]);  // instantaneous column code will be generated based on the current ldr states

  if (_inst_column_code_array[0] == "000") intmt_column_code[0] = _inst_column_code_array[0];  // intermediate column code will be set to 000 if instantaneous column code is the same
  // checking for max number of 1's, intermediate column code will be updated if the current sum is more then previous - that is to say new led has been detected on the same column
  if ((_LDR_current_state_array[0] + _LDR_current_state_array[1] + _LDR_current_state_array[2]) > (_LDR_previous_state_array[0] + _LDR_previous_state_array[1] + _LDR_previous_state_array[2])) {
    intmt_column_code[0] = _inst_column_code_array[0];
    // //add_log("intmt-column-code-left:" + intmt_column_code[i] + "_");
  }
  // final column code will be updated with previous intermediate column code if intermediate column code is 000 and if previous was not. this implies that bot has crossed a column. and also final column code array and ldr time array will also be updated accordingly
  if (prev_intmt_column_code[0] != "000" && intmt_column_code[0] == "000") {
    final_column_code[0] = prev_intmt_column_code[0];
    // add_log("final-column-code-left:" + final_column_code[0]);

    final_column_code_array[2] = final_column_code_array[1];
    final_column_code_array[1] = final_column_code_array[0];
    final_column_code_array[0] = final_column_code[0];

    final_column_code[0] = "000";

    _LDR_time_array[2] = _LDR_time_array[1];
    _LDR_time_array[1] = _LDR_time_array[0];
    _LDR_time_array[0] = millis();
  }
  prev_intmt_column_code[0] = intmt_column_code[0];

  // Right side code
  _inst_column_code_array[1] = String(_LDR_current_state_array[3]) + String(_LDR_current_state_array[4]) + String(_LDR_current_state_array[5]);  // instantaneous column code will be generated based on the current ldr states
  if (_inst_column_code_array[1] == "000") intmt_column_code[1] = _inst_column_code_array[1];                                                    // intermediate column code will be set to 000 if instantaneous column code is the same
  // checking for max number of 1's, intermediate column code will be updated if the current sum is more then previous - that is to say new led has been detected on the same column
  if ((_LDR_current_state_array[3] + _LDR_current_state_array[4] + _LDR_current_state_array[5]) > (_LDR_previous_state_array[3] + _LDR_previous_state_array[4] + _LDR_previous_state_array[5])) {
    intmt_column_code[1] = _inst_column_code_array[1];
    // //add_log("intmt-column-code-right:" + intmt_column_code[i] + "_");
  }

  // final column code will be updated with previous intermediate column code if intermediate column code is 000 and if previous was not. this implies that bot has crossed a column. and also final column code array and ldr time array will also be updated accordingly
  if (prev_intmt_column_code[1] != "000" && intmt_column_code[1] == "000") {
    final_column_code[1] = prev_intmt_column_code[1];
    // add_log("final-column-code-right:" + final_column_code[1]);

    final_column_code_array[5] = final_column_code_array[4];
    final_column_code_array[4] = final_column_code_array[3];
    final_column_code_array[3] = final_column_code[1];

    final_column_code[1] = "000";

    _LDR_time_array[5] = _LDR_time_array[4];
    _LDR_time_array[4] = _LDR_time_array[3];
    _LDR_time_array[3] = millis();
  }

  prev_intmt_column_code[1] = intmt_column_code[1];

  //the difference of time array between 1st and 3rd element is less then 500ms and more then 5ms its considered as a atation and left station detected flag will be set to true
  if ((_LDR_time_array[0] - _LDR_time_array[2]) < 500 && (_LDR_time_array[0] - _LDR_time_array[2]) > 5) {
    station_code[0] = final_column_code_array[2] + final_column_code_array[1] + final_column_code_array[0];
    add_log("Station-code-left:" + station_code[0] + " speed= " + String(90.0 / (_LDR_time_array[0] - _LDR_time_array[2])));
    left_station_detected = true;
    // _LED_detected = false;
    // _LED_detection_count = 0;
    _LDR_time_array[0] = 0;
    _LDR_time_array[1] = 0;
    _LDR_time_array[2] = 0;
  }
  //the difference of time array between 1st and 3rd element is less then 500ms and more then 5ms its considered as a atation and right station detected flag will be set to true

  if ((_LDR_time_array[3] - _LDR_time_array[5]) < 500 && (_LDR_time_array[3] - _LDR_time_array[5]) > 5) {
    station_code[1] = final_column_code_array[5] + final_column_code_array[4] + final_column_code_array[3];
    add_log("Station-code-right:" + station_code[1] + " speed= " + String(90.0 / (_LDR_time_array[3] - _LDR_time_array[5])));
    right_station_detected = true;
    // _LED_detected = false;
    // _LED_detection_count = 0;
    _LDR_time_array[3] = 0;
    _LDR_time_array[4] = 0;
    _LDR_time_array[5] = 0;
  }

  String station_code_main;

  // What happens when station is detected - majority of the logic in here
  if (left_station_detected || right_station_detected) {
    if (left_station_detected)  // some station was detected
    {
      left_station_detected = false;
      station_code_main = station_code[0];
      //add_log(" Station-code-left:" + station_code_main);
      station_code[0] = "000000000";
      mcp1.digitalWrite(LED1_output_pin, ledstatus);
      ledstatus = !ledstatus;
    }

    if (right_station_detected)  // some station was detected
    {
      right_station_detected = false;
      station_code_main = station_code[1];
      //add_log("Station-code-right:" + station_code_main);
      station_code[1] = "000000000";
      mcp1.digitalWrite(LED1_output_pin, ledstatus);
      ledstatus = !ledstatus;
    }

    // valid station verification of detected station
    for (int station_index = 0; station_index <= N_STATIONS; station_index++) {
      if (station_index == N_STATIONS) {
        add_log("invalid station - stop station code: " + station_code_main);
        bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(255, 0, 0));
        }
        carrier_led.show();
        break;
      }

      if (station_code_main == STATION_CODE_LIST[station_index]) {
        currentStation = STATION_ID_LIST[station_index];
        add_log("Read: " + currentStation);

        // Special cases after new station detected

        // At Infeed
        for (int i = 0; i < 4; i++) {
          if (currentStation == INFEED_LIST[i]) {
            for (int i = 0; i < 2; i++) {  // turn on 2 segment lights
              carrier_led_function(i, 1);
            }
            currentInfeed = currentStation;
            add_log("Infeed station: " + currentInfeed);
            add_log("infeed motor stoped");
            bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
            delay(1000);
            // add_log("flaps not pressed");
            // add_log("flaps pressed");
            localization_flag = false;
            station_count = 0;

            //bot makes a dummy request when ever it reaches any of 4 infeeds
            String sending_msg = "";
            if (currentInfeed == "I1") {
              sending_msg = BOT_ID + "_" + String(udp_request_id) + "_" + "I1" + "_" + "0" + "_" + "D1" + "_" + "D2" + "_" + "D3" + "_" + "0" + "_N_I1";
            } else if (currentInfeed == "I2") {
              sending_msg = BOT_ID + "_" + String(udp_request_id) + "_" + "I2" + "_" + "0" + "_" + "D11" + "_" + "D12" + "_" + "D13" + "_" + "0" + "_N_I1";
            } else if (currentInfeed == "I3") {
              sending_msg = BOT_ID + "_" + String(udp_request_id) + "_" + "I3" + "_" + "0" + "_" + "D43" + "_" + "D44" + "_" + "D45" + "_" + "0";
            } else if (currentInfeed == "I4") {
              sending_msg = BOT_ID + "_" + String(udp_request_id) + "_" + "I4" + "_" + "0" + "_" + "D31" + "_" + "D32" + "_" + "D33" + "_" + "0";
            }
            udp.beginPacket(UDP_IP, UDP_PORT);
            udp.print(sending_msg);
            udp.endPacket();

            is_infeed = true;

            //updating dropoff
            String dropoff_update_sections = "";
            for (int containerID = 0; containerID < 1; containerID++) {
              if (dropoff_state[containerID] == 1) {
                dropoff_update_sections = dropoff_update_sections + "-S" + String(containerID + 1);
              }
            }
            add_log(dropoff_update_sections);
            if (dropoff_update_flag) {
              dropoff_update_flag = false;
              int pktupdatehttpCode = 0;
              String pkt_update_url = "http://192.168.2.109/m-sort-distribution-server/UpdateParcelDroppedForBot?botId=" + BOT_ID + "&sectionIds=" + dropoff_update_sections;
              add_log(pkt_update_url);
              while (pktupdatehttpCode != HTTP_CODE_OK) {

                http.begin(pkt_update_url);
                pktupdatehttpCode = http.PUT(pkt_update_url);

                if (pktupdatehttpCode == HTTP_CODE_OK) {
                  String payload = http.getString();
                  add_log("Dropoff updated!");

                } else {
                  add_log(String(pktupdatehttpCode) + " : dropoff update failed ");
                  delay(500);
                }
              }
            } else {
              // flushing all containers
              add_log("flushing all containers because parcel drop off was unsuccessful!");
              String flushurl = "http://192.168.2.109/m-sort-distribution-server/FlushTheBot?botId=" + BOT_ID;  // confirm if the api takes 0-3 or 1-4 as compartmen id
              add_log(flushurl);
              bool flush_flag = false;
              while (!flush_flag) {
                http.begin(flushurl);
                int flushhttpCode = http.PUT(flushurl);
                add_log("InfeedHTTP resp : " + String(flushhttpCode));
                if (flushhttpCode > 0) {
                  if (flushhttpCode == HTTP_CODE_OK) {
                    add_log("Flush Successful!");
                    flush_flag = true;
                  } else {
                    add_log("Flush Failed!");
                    delay(500);
                  }
                } else {
                  add_log("Flush Failed!");
                  delay(500);
                }
              }
            }
            for (int containerID = 0; containerID < 1; containerID++) {  // reseting flap ldr state and dropoff state to 0
              flaps_ldr_state_values[containerID] = 0;
              dropoff_state[containerID] = 0;
            }

            //flushing all containers
            // String flushurl = "http://192.168.2.109/m-sort-distribution-server/FlushTheBot?botId=" + BOT_ID;  // confirm if the api takes 0-3 or 1-4 as compartmen id
            // add_log(flushurl);
            // bool flush_flag = false;
            // while (!flush_flag) {
            //   http.begin(flushurl);
            //   int flushhttpCode = http.PUT(flushurl);
            //   add_log("InfeedHTTP resp : " + String(flushhttpCode));
            //   if (flushhttpCode > 0) {
            //     if (flushhttpCode == HTTP_CODE_OK) {
            //       flush_flag = true;
            //     } else {
            //       delay(1000);
            //     }
            //   } else {
            //     delay(1000);
            //   }
            // }

            mcp1.digitalWrite(LED2_output_pin, 0);
            bool buttonpress_flag = false;
            uint8_t packetCount = 0;
            add_log("checking for pkts");

            while ((!buttonpress_flag)) {

              while (!mcp1.digitalRead(Panel_PB2_input_pin)) {  // on button press
                buttonpress_flag = true;
                for (int i = 0; i < 2; i++) {
                  carrier_led_function(i, 0);  // turning off the segment led
                }
              }
              int new_packet_index = ScanForFlapLdr();

              if (new_packet_index != -1) {
                add_log("segment: " + String(new_packet_index) + " detected");
                carrier_led_function(new_packet_index, 0);  //turning off occupied segement
                packetCount++;
                if (packetCount == 1) {  // check if all 4 segments are occupied
                  buttonpress_flag = true;
                }

                add_log(BOT_ID + "pkt count: " + String(packetCount));
                mcp1.digitalWrite(LED2_output_pin, 1);
                String infeedurl = "http://192.168.2.109/m-sort-distribution-server/GetDestinationStationIdForBot?botId=" + BOT_ID + "&infeedStationId=" + currentInfeed + "&sectionId=S" + String(new_packet_index + 1);  // confirm if the api takes 0-3 or 1-4 as compartmen id

                add_log(infeedurl);
                bool get_dest_flag = false;
                while (!get_dest_flag) {
                  http.begin(infeedurl);
                  int infeedhttpCode = http.GET();
                  add_log("InfeedHTTP resp : " + String(infeedhttpCode));
                  if (infeedhttpCode == HTTP_CODE_OK) {
                    get_dest_flag = true;
                    String payload = http.getString();
                    add_log(payload);
                    int arr_len;
                    String *split_data = split_string(payload, '-', arr_len);
                    dropoff_station_list[new_packet_index] = split_data[0];
                    planned_path_length = split_data[1].toInt();
                    for (int i = 0; i < 100; i++) {  //incrase planedpath length from 50 to 100 in updated 50 destination graph size
                      plannedPath[i] = "";
                    }
                    for (int i = 0; i < planned_path_length; i++) {
                      plannedPath[i] = split_data[i + 2];
                    }
                    exceptedStation = plannedPath[0];
                    delete[] split_data;
                  } else {
                    add_log("Get Destination failed!");
                    delay(500);  // if status code is not 200
                  }
                }
                mcp1.digitalWrite(LED2_output_pin, 0);
              }
            }
            break;  // to not compare current station with infeeds after it has matched
          }
        }

        // Localization
        if (localization_flag) {
          bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
          localization_flag = false;
          // flushing all containers
          add_log("flushing all containers");
          String flushurl = "http://192.168.2.109/m-sort-distribution-server/FlushTheBot?botId=" + BOT_ID;  // confirm if the api takes 0-3 or 1-4 as compartmen id
          add_log(flushurl);
          bool flush_flag = false;
          while (!flush_flag) {
            http.begin(flushurl);
            int flushhttpCode = http.PUT(flushurl);
            add_log("InfeedHTTP resp : " + String(flushhttpCode));
            if (flushhttpCode > 0) {
              if (flushhttpCode == HTTP_CODE_OK) {
                add_log("Flush Successful!");
                flush_flag = true;
              } else {
                add_log("Flush Failed!");
                delay(500);
              }
            } else {
              add_log("Flush Failed!");
              delay(500);
            }
          }



          String localize_url = "http://192.168.2.109/m-sort-distribution-server/GetPlannedPath?currentStationId=" + currentStation;
          http.begin(localize_url);
          int localizehttpCode = http.GET();

          if (localizehttpCode > 0) {
            if (localizehttpCode == HTTP_CODE_OK) {
              String payload = http.getString();
              add_log("Localized: " + payload);
              int arr_len;
              String *split_data = split_string(payload, '-', arr_len);
              planned_path_length = split_data[0].toInt();
              for (int i = 0; i < arr_len - 1; i++) {
                plannedPath[i] = split_data[i + 1];
              }
              exceptedStation = plannedPath[0];
              delete[] split_data;
            } else {
              add_log("Localization failed!");
              delay(500);
            }
          } else {
            add_log("Localization failed!");
            delay(500);
          }
        }

        //Drop-off Station
        for (uint8_t i = 0; i < 1; i++) {
          if (currentStation == dropoff_station_list[i]) {
            add_log("mb--> " + String(dropoff_motherbag_state[i]));
            if (dropoff_motherbag_state[i] == 1) {
              add_log("Motherbag Available!");
              dropoff_update_flag = true;
              add_log(BOT_ID + "drop station: " + currentStation + " segment: " + i);
              mcp2.digitalWrite(segment_flap_emg[0], 1);  // open flap based on which drop off is detected
              mcp2.digitalWrite(segment_flap_emg[1], 1);  // open flap based on which drop off is detected
              dropoff_state[i] = 1;
            } else {
              add_log("Motherbag NOT Available!");
            }
          }
        }

        // Electromagnet check at I11 I21 I31 I41
        if (currentStation == "I11" || currentStation == "I21" || currentStation == "I31" || currentStation == "I41") {
          add_log("emg on station: " + currentStation);
          //TODO check for flaps if they are open and check if dropoff_update_flag is true and set it to false if the flaps are not open
          for (int i = 0; i < 2; i++) {
            mcp2.digitalWrite(segment_flap_emg[i], 0);  // turned on the electromagnet just at the automatic flap closure mechanism
          }
        }

        // general code to be run after new station detected
        if (currentStation == exceptedStation) {
          station_count++;
          exceptedStation = plannedPath[station_count];
          add_log("Exp: " + exceptedStation);

          // diverting after new station based on expected station
          if (exceptedStation == RIGHT_STATION_ID_LIST[station_index]) {
            add_log("Go_Right");
            servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
            mcp1.digitalWrite(DIV_Relay_input_pin, 0);
          } else if (exceptedStation == LEFT_STATION_ID_LIST[station_index]) {
            add_log("Go_Left");
            servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
            mcp1.digitalWrite(DIV_Relay_input_pin, 0);
          } else {
            // bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
            add_log("wrong diverting - stop");
          }

          // UDP request after station detected and diverter handled
          int udp_responce_len = 0;
          bool udp_resp_flag = false;
          int station_string_arr_len = 0;

          if (is_infeed) {
            add_log("At infeed, path is done, starting motor for 200ms");
            if (DIV_FLAG == true) {
              bldc.actuate(BLDC_Motor::_GO_FORWARD, BLDC_Motor::_PWM_S2);
            }
            delay(500);
            bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
            add_log("Motor stopped");
            is_infeed = false;
          }

          if (exceptedStation == dropoff_station_list[0]) {
            check_motherbag_availability(0);
          }

          while (!udp_resp_flag) {
            String udp_responce_string = UDP_req_resp();  // UDP request/response getting printed in here - every request/response is printed
            String *udp_responce_string_arr = split_string(udp_responce_string, '_', udp_responce_len);

            if (udp_responce_len > 2) {  // received the station_string as well
              String *station_string_arr = split_string(udp_responce_string_arr[4], '-', station_string_arr_len);
              // add_log(" station_string_arr : "+ station_string_arr[0]+ "-"+station_string_arr[1]+"-"+station_string_arr[2]+"-"+station_string_arr[3]);

              if (udp_responce_string_arr[2] == "P") {
                bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
                delay(500);
              }

              if (station_string_arr_len > 1) {

                // rule N logic
                if (udp_responce_string_arr[2] == "N") {
                  if (station_string_arr[1] == exceptedStation || station_string_arr[2] == exceptedStation) {
                    udp_resp_flag = true;
                    // if (bldc.motor_status == 0) {
                    if (DIV_FLAG == true) {
                      bldc.actuate(BLDC_Motor::_GO_FORWARD, BLDC_Motor::_PWM_S2);
                    }
                    // }
                  } else {
                    // if (bldc.motor_status == 1) {
                    bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
                    delay(500);
                    // }
                  }
                }

                // rule R logic
                else if (udp_responce_string_arr[2] == "R") {  //todo change the dropoff of api of redirection
                  add_log(" Redirecting..");
                  bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
                  for (int i = 0; i < 100; i++) {  //incrase planedpath length from 50 to 100 in updated 50 destination graph size
                    plannedPath[i] = "";
                  }
                  // Extract new plannedpath from station_string_arr --> current station to redirection station, update temp_dropoff_station if drop_station_id is in the new planned path
                  planned_path_length = 0;
                  String _temp_dropoff_station = drop_station_id;
                  String plannedPath_print_string = "";
                  for (int i = 0; i < station_string_arr_len; i++) {
                    if (station_string_arr[i] == currentStation) {
                      for (int j = i; j < station_string_arr_len; j++) {
                        plannedPath[j - i] = station_string_arr[j];
                        plannedPath_print_string = plannedPath_print_string + "-" + plannedPath[j - i];
                        planned_path_length++;
                        for (int k = 0; k < 4; k++) {
                          if (station_string_arr[j] == drop_station_id) {
                            _temp_dropoff_station = "X";
                          }
                        }
                      }
                      add_log("Redirected substring: " + plannedPath_print_string);

                      break;  // exit the for if the current station is found
                    }
                  }

                  // String _temp_dropoff_station = drop_station_id;
                  // if (currentStation == station_string_arr[0]) {
                  //   for (int i = 0; i < station_string_arr_len; i++) {
                  //     plannedPath[i] = station_string_arr[i];
                  //     if (plannedPath[i] == _temp_dropoff_station) {
                  //       _temp_dropoff_station = "X";
                  //     }
                  //     add_log(plannedPath[i]);
                  //     planned_path_length++;
                  //   }
                  // } else if (currentStation == station_string_arr[1]) {
                  //   for (int i = 0; i < station_string_arr_len - 1; i++) {
                  //     plannedPath[i] = station_string_arr[i + 1];
                  //     if (plannedPath[i] == _temp_dropoff_station) {
                  //       _temp_dropoff_station = "X";
                  //     }
                  //     add_log(plannedPath[i]);
                  //     planned_path_length++;
                  //   }
                  // }

                  bool redirectionHTTPflag = false;
                  while (!redirectionHTTPflag) {
                    //TODO change api
                    String redirectionURL = "http://192.168.2.109/m-sort-distribution-server/GetPlannedPathForRedirection?currentStationId=" + plannedPath[planned_path_length - 1] + "&droppingStationId=" + _temp_dropoff_station + "&infeedStationId=" + currentInfeed;
                    http.begin(redirectionURL);
                    int redirectionhttpCode = http.GET();

                    if (redirectionhttpCode == HTTP_CODE_OK) {
                      redirectionHTTPflag = true;
                      String payload = http.getString();
                      add_log("Redirection API: " + payload);
                      int arr_len;
                      String *split_data = split_string(payload, '-', arr_len);
                      // add the smaller plannedPath substring extracted earlier to the plannedPath returned by the API from redirecting station to infeed
                      for (int i = 0; i < arr_len - 2; i++) {
                        plannedPath[planned_path_length] = split_data[i + 2];
                        plannedPath_print_string = plannedPath_print_string + "-" + plannedPath[planned_path_length];
                        planned_path_length++;
                      }
                      add_log("Redirected Final plannePath: " + plannedPath_print_string);

                      delete[] split_data;
                    } else {
                      add_log("redirection http status: " + String(redirectionhttpCode));
                      delay(500);  // next http request after 500 ms if the first one didnt return status ok
                    }
                  }
                  // reset current station and expected station after updating planned path
                  currentStation = plannedPath[0];
                  exceptedStation = plannedPath[1];
                  station_count = 1;  // pointer to the expected station, so imp to set it to 1. On next station detected, if current station=expected this would be incremented to 2 immediately

                  // divert after getting R - needed only for the case where you are already at flow control but doesn't hurt even if done for all R cases
                  add_log(" R Rule diversion");
                  if (exceptedStation == RIGHT_STATION_ID_LIST[station_index]) {
                    add_log("Go_Right");
                    servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
                    mcp1.digitalWrite(DIV_Relay_input_pin, 0);
                  } else if (exceptedStation == LEFT_STATION_ID_LIST[station_index]) {
                    add_log("Go_Left");
                    servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
                    mcp1.digitalWrite(DIV_Relay_input_pin, 0);
                  } else {
                    bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
                    add_log("wrong diverting - stop");
                  }
                  delay(500);  // giving time for diverter to switch before moving forward
                  add_log("R-start motor");
                  if (DIV_FLAG == true) {
                    bldc.actuate(BLDC_Motor::_GO_FORWARD, BLDC_Motor::_PWM_S2);
                  }


                  udp_resp_flag = true;  // once new planned path is received after diversion through R command, this is set to true to come out of udp request loop
                }
              } else  // received response only had occupied station in that case - stop motor and resend the request after a delay
              {
                add_log("UDP response station string has only 0/1 station - stop");
                bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
                delay(500);  // make new request after 500 ms
              }
              delete[] station_string_arr;
            } else  // station_string (on index 4) has not been received - only on first udp request after bot is started
            {
              add_log("UDP response - No station string received - stop");
              //bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
              delay(500);  // make new request after 500 ms
            }
            delete[] udp_responce_string_arr;
          }  // end of udp request while loop

        } else  // diverting rules when bot is at a special station not listed in planned path -> so it turns out to be an unexpected station
        {
          add_log("unexpected station - stop current station: " + currentStation + " expexted station: " + exceptedStation);
          bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
          for (int i = 0; i <= 27; i++) {
            carrier_led.setPixelColor(i, carrier_led.Color(0, 0, 255));
          }
          carrier_led.show();
        }
        break;  // exit the valid station verification for-loop after finding the station id
      }
    }  // end of valid station verification for-loop
  }

  if (!_udp_looptime_flag) {
    if (_loop_count < 5000) {
      _loop_count++;
      _loop_time = millis() - _loop_start_time;
      _max_loop_time = _max_loop_time + _loop_time;
    } else {
      _loop_count = 0;
      float _time_diff = _max_loop_time / 5000;
      //add_log("loop time " + String(_time_diff));
      _max_loop_time = 0;
    }
  }
}

int ScanForFlapLdr() {
  for (uint8_t i = 0; i < 1; i++) {

    set_select_pin(Flap_ldr_pin_array[1], 2);
    flaps_ldr_raw_values[i] = map(analogRead(Mux2_ADC_PIN), 0, 4096, 0, 1024);

    if (flaps_ldr_state_values[i] == 0 && flaps_ldr_raw_values[i] > FLAP_LDR_ADC_UPPER_LIMIT) {
      flaps_ldr_state_values[i] = 1;
      return i;
    }
    // return 0;
    //  else if (flaps_ldr_state_values[i] == 1 && flaps_ldr_raw_values[i] < FLAP_LDR_ADC_LOWER_LIMIT) {
    //   flaps_ldr_state_values[i] = 0;
    // }
  }
  return -1;
}
void HttpEvent(HttpEvent_t *event) {
  switch (event->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("Http Event Error");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      Serial.println("Http Event On Connected");
      break;
    case HTTP_EVENT_HEADER_SENT:
      Serial.println("Http Event Header Sent");
      break;
    case HTTP_EVENT_ON_HEADER:
      Serial.printf("Http Event On Header, key=%s, value=%s\n", event->header_key, event->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      break;
    case HTTP_EVENT_ON_FINISH:
      Serial.println("Http Event On Finish");
      break;
    case HTTP_EVENT_DISCONNECTED:
      Serial.println("Http Event Disconnected");
      break;
  }
}

void update_firmware() {
  HttpsOTA.onHttpEvent(HttpEvent);
  add_log("Starting OTA");
  HttpsOTA.begin(HTTP_OTA_SERVER_URL, server_certificate, true);
  add_log("Please Wait it takes some time ...");

  while (true)  // only prinitng otastatus every 1s- ota file already taken
  {
    otastatus = HttpsOTA.status();
    add_log(". ");
    if (otastatus == HTTPS_OTA_SUCCESS) {
      add_log("Firmware written successfully");
      ESP.restart();
    } else if (otastatus == HTTPS_OTA_FAIL) {
      add_log("Firmware Upgrade Fail");
      return;
    }
    delay(1000);
  }
}

String *split_string(String &path, char delimiter, int &length) {
  length = 1;
  bool found_delim = false;
  for (int i = 0; i < path.length(); i++) {
    if (path[i] == delimiter) {
      length++;
      found_delim = true;
    }
  }
  if (found_delim) {
    String *arr_elements = new String[length];
    int i = 0;
    for (int itemIndex = 0; itemIndex < length; itemIndex++) {
      for (; i < path.length(); i++) {

        if (path[i] == delimiter) {
          i++;
          break;
        }
        arr_elements[itemIndex] += path[i];
      }
    }
    return arr_elements;
  }
  return nullptr;
}

void check_motherbag_availability(int upcoming_dropoff) {
  add_log("checking motherbag: " + dropoff_station_list[upcoming_dropoff]);
  String check_motherbag_url = "http://192.168.2.109/m-sort-distribution-server/CheckIfStationIsAvailableAsDropOff?stationId=" + dropoff_station_list[upcoming_dropoff];
  add_log(check_motherbag_url);
  bool check_motherbag_flag = true;
  while (check_motherbag_flag) {
    http.begin(check_motherbag_url);
    int checkMotherbagHttpCode = http.GET();
    if (checkMotherbagHttpCode == HTTP_CODE_OK) {
      String checkMotherbagResponse = http.getString();
      add_log(checkMotherbagResponse);
      if (checkMotherbagResponse == "true") {
        dropoff_motherbag_state[upcoming_dropoff] = 1;
      } else {
        dropoff_motherbag_state[upcoming_dropoff] = 0;
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(255, 255, 0));
        }
        carrier_led.show();
      }
      check_motherbag_flag = false;
    }
  }
}

String UDP_req_resp() {
  bool _udp_flag = true;
  int _udp_responce = 0;
  int UDP_TIMEOUT = 200;
  _udp_looptime_flag = true;
  int iter = 0;
  while (_udp_flag) {

    //Request logic for UDP 2.4
    iter++;
    add_log("Reconnection Iterator: " + String(iter));
    udp_request_id++;
    String sending_msg = BOT_ID + "_" + String(udp_request_id) + "_" + currentStation + "_" + "0" + "_" + exceptedStation;
    if (station_count + 2 <= planned_path_length - 1) {
      sending_msg = sending_msg + "_" + plannedPath[station_count + 1] + "_" + plannedPath[station_count + 2];
    } else if (station_count + 1 <= planned_path_length - 1) {
      sending_msg = sending_msg + "_" + plannedPath[station_count + 1] + "_" + "X";
    } else {
      sending_msg = sending_msg + "_" + "X" + "_" + "X";
    }
    sending_msg = sending_msg + "_" + String(bldc.motor_status) + "_N_" + plannedPath[planned_path_length - 1];

    add_log(sending_msg);
    udp.beginPacket(UDP_IP, UDP_PORT);
    udp.print(sending_msg);
    udp.endPacket();

    pkt_received_time = 0;
    while (pkt_received_time < UDP_TIMEOUT) {
      char incomingPacket[255];
      int packetSize = udp.parsePacket();
      if (packetSize) {
        int len = udp.read(incomingPacket, 255);
        incomingPacket[len] = 0;

        add_log("Heap Free: " + String(ESP.getFreeHeap()));

        String incomingPacketStr = String(incomingPacket);
        add_log(incomingPacketStr);
        int index = incomingPacketStr.indexOf('_');  // gives index of 1st underscore
        int length = incomingPacketStr.length();
        String response_id_str = incomingPacketStr.substring(0, index);
        String request_id_str = String(udp_request_id);
        // if (response_id_str == "KILL") {
        //   bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
        //   mcp.digitalWrite(EMERGENCY_PB_OUTPUT, 1);
        // }
        // if (response_id_str == "OTA") {
        //   add_log("ota triggered via udp");
        //   update_firmware();
        // }
        if (response_id_str == request_id_str) {
          return incomingPacketStr;
        }
      }
    }

    bldc.actuate(BLDC_Motor::_STOP, BLDC_Motor::_PWM_S5);
    UDP_TIMEOUT = 500;
  }
}
void carrier_led_function(int segment_id, bool state) {
  switch (segment_id) {
    case 0:
      if (state) {
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(255, 255, 255));
        }
        carrier_led.show();
      } else {
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(0, 0, 0));
        }
        carrier_led.show();
      }
      break;
    case 1:
      if (state) {
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(255, 255, 255));
        }
        carrier_led.show();
      } else {
        for (int i = 0; i <= 27; i++) {
          carrier_led.setPixelColor(i, carrier_led.Color(0, 0, 0));
        }
        carrier_led.show();
      }
      break;
  }
}

// void flowControlDivertion(String station1, String station2) {
//   if (station1 == "S11" && station2 == "S14") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S11" && station2 == "D7") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S14" && station2 == "S15") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S14" && station2 == "D37") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S15" && station2 == "C3") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S15" && station2 == "S17") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S24" && station2 == "S25") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S24" && station2 == "C6") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S25" && station2 == "C2") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S25" && station2 == "S26") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S35" && station2 == "C11") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S35" && station2 == "S36") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S24" && station2 == "S35") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S24" && station2 == "C7") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S41" && station2 == "S44") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S41" && station2 == "D25") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S44" && station2 == "S45") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S44" && station2 == "D13") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S45" && station2 == "S47") {
//     servo_diverter.write(DIV_ACTUATE_RIGHT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   } else if (station1 == "S45" && station2 == "C10") {
//     servo_diverter.write(DIV_ACTUATE_LEFT_ANGLE);
//     mcp.digitalWrite(DIV_Relay_input_pin, 0);
//   }
// }
// int disableOnTrigger() {
//   float fb_adc_value = analogRead(DIV_Feedback_input_pin);
//   _fb_angle = map(fb_adc_value, FB_ADC_LOWER_LIMIT, FB_ADC_UPPER_LIMIT, 0, 180);
//   if (_timer < _TIMEOUT && diverter_relay_flag) {
//     if ((_command == GO_LEFT && (_fb_angle > (_ACTUATE_LEFT_ANGLE - _ACTUATE_BUFFER_ANGLE))) || (_command == _GO_RIGHT && _fb_angle < (_ACTUATE_RIGHT_ANGLE + _ACTUATE_BUFFER_ANGLE))) {
//       _current_diverter_position = _command;
//       _stop_time = millis();
//