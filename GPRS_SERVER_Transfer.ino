#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

String payload_data;

//const char* GetCheckURL = "http://116.74.138.184:8053/firmware/version/";
const char* GetCheckURL = "http://116.74.138.184:3000/manage-users";
const char* PostCheckURL = "http://116.74.138.184:3000/api/consumption";

const int LED_PIN = 2;
HardwareSerial NodeMCU(1);  // Serial port for GPRS (RX: 3, TX: 1)

String APN = "bsnlnet";

void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}

void checkGprsGetAPICall() {
  String url = GetCheckURL;
//  Handle GPRS request here (using send_data_to_server function)
  int httpCode = send_data_to_server(url, "GET", "", "", "");

  if (httpCode == 200) {
    Serial.println("Response Data: " + payload_data);
  } else {
    Serial.println("GET Request failed. HTTP code: " + String(httpCode));
  }
  
}

void checkGprsPostAPICall() {
    String url = PostCheckURL;
    DynamicJsonDocument doc(1024); // Adjust size as needed
    
    doc["mac_Id"] = "MAC00123E";
    doc["time_stamp"] = "2025-03-21T16:59:00Z";
    doc["consumption"] = 00;
    doc["relay_status"] = "OFF";
    doc["voltage"] = 240;
    doc["current"] = 1.4;
    doc["power"] = 245;
    doc["power_factor"] = 1;
    doc["balance"] = 500;
    doc["frequency"] = 50;
    doc["tamper_detected"] = false;
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    
    int httpCode = send_data_to_server(url, "POST", "application/json", "", jsonPayload);
    
    if (httpCode == 200) {
        Serial.println("Response Data: " + payload_data);
    } else {
        Serial.println("POST Request failed. HTTP code: " + String(httpCode));
    }
}

// GPRS communication functions provided
bool get_gprs_sim_status() {
  Serial.println("Read SIM Status");
  Serial.println(send_data_to_gprs("AT+CPIN=?", "+CPIN:", 20000));
  return true;
}

int get_gprs_signal_status() {
  int rssi;
  String RSSI_Res;
  Serial.println("Read RSSI Status");

  RSSI_Res = send_data_to_gprs("AT+CSQ", "+CSQ:", 20000);
  Serial.println("RSSI: " + RSSI_Res);

  // Return RSSI value
  return rssi;
}

int parse_gprs_status(String payload) {
  int http_status = -1, payload_len;
  int from, toindex, payload_index;
  String str, len, pl, pl1;
  from = payload.indexOf(",");
  pl = payload.substring(from + 1, payload.length() - 2);
  toindex = pl.indexOf(",");
  str = pl.substring(0, toindex);
  len = pl.substring(toindex + 1, payload.length() - 1);

  http_status = str.toInt();
  payload_len = len.toInt();
  Serial.printf("[status,length]=[%d,%d]\n", http_status, payload_len);
  return http_status;
}

String parse_gprs_payload(String payload) {
  String payload1, msglength;

  int header, from, toindex, len;
  Serial.println("HTTP status code data: " + payload);
  header = payload.indexOf("+HTTPREAD: ");
  from = payload.indexOf(";{");
  msglength = payload.substring(header + 10, from - 1);
  Serial.println("HTTP PL: " + msglength);
  toindex = payload.indexOf("};", from);
  len = msglength.toInt();
  Serial.printf("[Start-End]=[%d,%d], %d\n", from, toindex, len);
  payload1 = payload.substring(from + 1, from + len + 1);
  Serial.println("HTTP PL: " + payload1);
  return payload1;
}

String send_data_to_gprs(String cmd, String expected_str, long millisec) {
  bool data_received = false;
  long exittime;
  int i;
  String response_str;
  Serial.println("[GSM CMD]:" + cmd);
  NodeMCU.flush();
  NodeMCU.println(cmd);
  exittime = millis() + millisec;
  char response[1000];  // Adjust size as needed
  memset(&response[0], 0, sizeof(response));  // empty response buff
  while (!data_received) {
    time_t completion_time = millis() + 500;
    while (millis() < completion_time);
    for (i = 0; NodeMCU.available() > 0 && i < 1000; i++) {
      response[i] = NodeMCU.read();
      if (response[i] == '\n') {
        response[i] = ';';
      }
    }
    response[i] = '\n';

    response_str = String(response);
    char charBuf[50];
    expected_str.toCharArray(charBuf, 50);

    if (strstr(response, charBuf)) {
      data_received = true;
      NodeMCU.println("[GSM RESP]: " + response_str);
    } else if (strstr(response, "ERROR")) {
      data_received = true;
      Serial.println("[GSM RESP]: ERROR");
    }
    if (exittime < millis()) {
      data_received = true;
      Serial.println("[GSM WARN] Time Out occurred..");
    }
  }
  return response_str;
}

bool connect_to_gprs() {
  bool code = false;
  String payload1;

  payload1 = send_data_to_gprs("AT", "OK", 4000);
  payload1 = send_data_to_gprs("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 4000);
  payload1 = send_data_to_gprs("AT+SAPBR=3,1,\"APN\"," + APN, "OK", 4000);
  payload1 = send_data_to_gprs("AT+SAPBR=1,1", "OK", 4000);
  payload1 = send_data_to_gprs("AT+SAPBR=2,1", "+SAPBR: ", 10000);
  int index = payload1.indexOf("0.0.0.0");
  if (index > 20) {
    code = false;
    Serial.printf("Failed To Connect To APN = %d\n", index);
  } else {
    code = true;
    Serial.printf("Connected To APN = %d\n", index);
  }
  return code;
}

int send_data_to_server(String url, String req_method, String ContentType, String token, String client_data) {
  int statusCode = -1;
  String payload1;
  get_gprs_signal_status();
  bool code = connect_to_gprs();
  payload1 = send_data_to_gprs("AT+HTTPINIT", "OK", 4000);
  payload1 = send_data_to_gprs("AT+HTTPPARA=\"CID\",1", "OK", 4000);
  payload1 = send_data_to_gprs("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 20000);
//  payload1 = send_data_to_gprs("AT+HTTPSSL=1", "OK", 4000); // Enable SSL if using HTTPS
 payload1 = send_data_to_gprs("AT+HTTPPARA=\"REDIR\",1", "OK", 4000); // Enable redirects
  
  if (!ContentType.isEmpty()) {
    payload1 = send_data_to_gprs("AT+HTTPPARA=\"CONTENT\",\"" + ContentType + "\"", "OK", 20000);
  }
  
  if (!token.isEmpty()) {
    String str = "Authorization:" + token;
    payload1 = send_data_to_gprs("AT+HTTPPARA=\"USERDATA\",\"" + str + "\"", "OK", 4000);
  }
  
  if (req_method == "POST") {
    payload1 = send_data_to_gprs("AT+HTTPDATA=" + String(client_data.length()) + ",100000", "DOWNLOAD", 20000);
    payload1 = send_data_to_gprs(client_data, "OK", 20000);
    payload1 = send_data_to_gprs("AT+HTTPACTION=1", "+HTTPACTION: ", 120000);
     statusCode = parse_gprs_status(payload1);
    if (statusCode == 0) {
      statusCode = -1;
    }
    payload1 = send_data_to_gprs("AT+HTTPREAD", "+HTTPREAD: ", 50000);
    payload_data = parse_gprs_payload(payload1);
  } else if (req_method == "GET") {
    payload1 = send_data_to_gprs("AT+HTTPACTION=0", "+HTTPACTION: ", 120000);
    statusCode = parse_gprs_status(payload1);
    if (statusCode == 0) {
      statusCode = -1;
    }
    payload1 = send_data_to_gprs("AT+HTTPSTATUS?", "+HTTPSTATUS:", 4000);
    payload1 = send_data_to_gprs("AT+HTTPREAD", "+HTTPREAD: ", 120000);
    payload_data = parse_gprs_payload(payload1);
  } else {
    Serial.println("Incorrect method.. ");
  }
  
  payload1 = send_data_to_gprs("AT+HTTPTERM", "OK", 10000);
  
  return statusCode;
}

void setup() {
  Serial.begin(115200);     // Serial for debugging
  NodeMCU.begin(9600, SERIAL_8N1, 3, 1); // GPRS communication

  pinMode(LED_PIN, OUTPUT);  // LED pin setup

  Serial.println("GPRS Test started...");

  // GPRS setup
  if (connect_to_gprs()) {
    Serial.println("GPRS connected successfully.");
    // checkGprsGetAPICall();
    checkGprsPostAPICall();
  } else {
    Serial.println("GPRS connection failed.");
    Serial.println("Retrying to connect to GPRS");
    connect_to_gprs();
  }
}

void loop() {
  blinkLED();
}