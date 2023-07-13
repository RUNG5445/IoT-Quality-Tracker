#include <BLEDevice.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <TinyGsmClientSIM7600.h>

#define SerialAT Serial1
#define SerialMon Serial

#define MIN_SPACE 500000
#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 26
#define PIN_RX 27
#define PWR_PIN 4
#define PIN_RST 5
#define PIN_SUP 12
#define BAT_ADC 35

#define apiKey (String) "REPLACE_WITH_YOUR_API_KEY"

#define FTPS_ADDR "REPLACE_WITH_FTPS_ADDRESS"
#define FTPS_PRT REPLACE_WITH_FTPS_PORT
#define FTPS_PATH "REPLACE_WITH_FTPS_PATH"
#define FTPS_USRN "REPLACE_WITH_YOUR_USERNAME"
#define FTPS_PASS "REPLACE_WITH_YOUR_PASSWORD"
#define FTPS_TYPE 1



String device_id = "Node 1";
String milist[] = {"A4:C1:38:30:D3:A3", "A4:C1:38:EF:1C:30"};
const int mi_num = sizeof(milist) / sizeof(milist[0]);
String Filename;
String filenameno;
double sleepmin = 20;

TinyGsmSim7600 modem(SerialAT);
TinyGsmSim7600::GsmClientSim7600 client(modem);
BLEClient *pClient;

struct SENSOR_DATA
{
  String temp[mi_num]; 
  String humi[mi_num]; 
};
SENSOR_DATA tempandhumi;

struct DATAFROMSENSER
{
  String temp; 
  String humi; 
};
DATAFROMSENSER sensordata;

// RTC information
struct RTC_INFO
{
  String date = "";
  String time = "";
  String offset = "";
};
RTC_INFO rtc_info;

// Network information
struct NETWORK_INFO
{
  // Location
  String date; 
  String time; 
  String lat;  
  String lon;  

  // Cell site information
  char type[10];
  char mode[10];
  String mcc;
  String mnc;
  int lac = 0;
  String cid;
  char freq_b[15];
  double rsrq = 0;
  double rsrp = 0;
  double rssi = 0;
  int rssnr;
};
NETWORK_INFO networkinfo;

// Battery information
struct BATT_INFO
{
  String batt_volt;
  String batt_level;
};
BATT_INFO battinfo;

// The remote service we wish to connect to.
static BLEUUID serviceUUID("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");

// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");

void decrypted(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  int16_t tmp_data = (pData[0] | (pData[1] << 8));
  sensordata.temp = ((float)tmp_data * 0.01);
  sensordata.humi = (float)pData[2];
}

void connectToSensor(BLEAddress pAddress)
{
  pClient = BLEDevice::createClient();
  pClient->connect(pAddress);
  delay(200);
  if (pClient->isConnected())
  {
    BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
    }

    BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr)
    {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
    }

    pRemoteCharacteristic->registerForNotify(decrypted); // Call decrypt here
    delay(5000);                                         //(5000)
    pClient->disconnect();
    delay(2000); // (2000)
  }
  delete pClient; // https://github.com/espressif/arduino-esp32/issues/3335
}

void modemPowerOn()
{
  const int SUPPLY_PIN = PIN_SUP;
  const int RESET_PIN = PIN_RST;
  const int POWER_PIN = PWR_PIN;

  // Set supply pin as output and turn it on
  pinMode(SUPPLY_PIN, OUTPUT);
  digitalWrite(SUPPLY_PIN, HIGH);

  // Set reset pin as output, reset modem and wait
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);
  delay(3000);
  digitalWrite(RESET_PIN, LOW);

  // Set power pin as output, power on modem and wait
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  delay(100);
  digitalWrite(POWER_PIN, HIGH);
  delay(1000);
  digitalWrite(POWER_PIN, LOW);
}

// Function to read battery level
void readBattLevel()
{
  SerialMon.println("\n----------   Start of readBattLevel()   ----------\n");
  const int numOfReadings = 10000;
  const float batteryFullVoltage = 4.12;
  const float batteryOffVoltage = 2.5;

  int battReadingsSum = 0;
  for (int i = 0; i < numOfReadings; i++)
  {
    battReadingsSum += analogRead(BAT_ADC);
  }

  float battVoltage = ((float)battReadingsSum / (float)numOfReadings) * 3.3 * 2 / 4096.0;
  float battPercentage = 100.0 * (1.0 - ((batteryFullVoltage - battVoltage) / (batteryFullVoltage - batteryOffVoltage)));

  battinfo.batt_level = String(battPercentage, 2);
  battinfo.batt_volt = String(battVoltage, 2);
  Serial.println("batt_level : " + battinfo.batt_level);
  Serial.println("batt_volt : " + battinfo.batt_volt);
  SerialMon.println("\n----------   End of readBattLevel()   ----------\n");
}

String sendAT(String command, int interval, boolean debug)
{
  String response = "";

  // Send command to AT module
  SerialAT.println(command);

  // Start a timer to wait for the response
  long int startTime = millis();

  // Wait for response for the specified interval
  while (((millis() - startTime)) < interval)
  {

    // Check if there is any data available from the AT module
    while (SerialAT.available() > 0)
    {
      int readData = SerialAT.read();
      response += char(readData); // Add the received data to the response string
    }
  }

  // Clear any remaining data from the AT module
  SerialAT.flush();

  // Print the response if debugging is enabled
  if (debug)
  {
    SerialMon.print(response);
  }

  // Return the response string
  return response;
}

void connect2LTE()
{
  SerialMon.println("\n----------   Start of connect2LTE()   ----------\n");
  // Set debug mode to true
  boolean DEBUG = 1;

  // Step 1: Close network
  SerialMon.println("\n1. Close network\n");
  delay(1000);
  sendAT("AT+NETCLOSE", 8000, DEBUG);

  // Step 2: Check SIM insertion
  SerialMon.println("\n2. Check SIM insertion\n");
  delay(1000);
  sendAT("AT+CPIN?", 5000, DEBUG);

  // Step 3: Open network
  SerialMon.println("\n3. Open network\n");
  delay(1000);
  sendAT("AT+CSOCKSETPN=1", 5000, DEBUG);
  sendAT("AT+NETOPEN", 5000, DEBUG);
  sendAT("AT+IPADDR", 5000, DEBUG);

  // Step 4: Ping checking
  SerialMon.println("\n4. Ping checking\n");
  delay(1000);
  sendAT("AT+CPING=\"www.google.co.th\",1,4", 10000, DEBUG);
  // AT+CPING=<dest_addr>,<dest_addr_type>, < num_pings>
  // response : +CPING: 3,4,4,0,55,80,66
  // response :
  //<result_type>,<num_pkts_sent>,<num_pkts_recvd>,<num_pkts_lost>,<min_rtt>,<max_rtt>, < avg_rtt>
  SerialMon.println("\n----------   End of connect2LTE()   ----------\n");
}

void listAllFile()
{
  // Begin SPIFFS in read-only mode
  if (!SPIFFS.begin(true))
  {
    SerialMon.println("An error occurred while mounting SPIFFS");
    return;
  }

  // Open the root folder
  File folder = SPIFFS.open("/");

  // Get the first file in the folder
  File file = folder.openNextFile();

  // Loop through all the files in the folder and print their names
  while (file)
  {
    SerialMon.print("FILE name : ");
    SerialMon.println(file.name());
    file = folder.openNextFile();
  }
}

bool writelog(String filename, String data2write)
{
  SerialMon.println("\n----------   Start of writelog()   ----------\n");
  char mode[5];
  String selectedfile;

  // 1. Begin SPIFFS
  SPIFFS.begin();

  // Check if SPIFFS is mounted successfully
  if (!SPIFFS.begin())
  {
    SerialMon.println("An error occurred while mounting SPIFFS");
    return false;
  }

  // 2. Check SPIFFS's available space. If there's not enough space, delete all files except .conf file.
  if (int(SPIFFS.totalBytes() - SPIFFS.usedBytes()) < MIN_SPACE)
  {
    SerialMon.println("SPIFFS does not have enough space.");

    // Open the root folder
    File folder = SPIFFS.open("/");

    // Get the first file in the folder
    File file = folder.openNextFile();

    // Delete all files except .conf file
    while (file)
    {
      SerialMon.println(file.name());
      selectedfile = String(file.name());
      SPIFFS.remove(file.name());
      file = folder.openNextFile();
    }

    // Restart the device
    ESP.restart();
    return false;
  }

  // List all files in SPIFFS
  listAllFile();

  // 3. Check if the file exists. If it doesn't, open the file in "write" mode. If it does, open the file in "append" mode.
  if (!SPIFFS.exists(filename))
  {
    sprintf(mode, "w");
  }
  else
  {
    sprintf(mode, "a");
  }

  // Wait for 100ms
  delay(100);

  // 4. Open the file and write data to it.
  File file = SPIFFS.open(filename, mode);
  file.println(data2write);

  // 5. Close the file
  file.close();

  // 6. Unmount SPIFFS
  SPIFFS.end();
  SerialMon.println("Write file successfully");
  SerialMon.println("\n----------   End of writelog()   ----------\n");
  // Return true to indicate that the operation was successful
  return true;
}

void readLocation()
{
  SerialMon.println("\n----------   Start of readLocation()   ----------\n");
  // Send AT command and retrieve location information
  String sentence = sendAT("AT+CLBS=4", 10000, 1);

  // Extract location data from the response
  int startIndex = sentence.indexOf("+CLBS: ");
  sentence = sentence.substring(startIndex + 7, startIndex + 55);
  sentence.replace("\r", "");
  sentence.replace("\n", "");
  sentence.replace("AT+CLBS=4ERROR", "");
  sentence.replace("AT+CPSI?+CPSI: NO SERVICE", "");

  // Parse the comma-separated values and store in an array
  int commaIndex = 0;
  int lastCommaIndex = 0;
  String values[6];
  for (int i = 0; i < 6; i++)
  {
    commaIndex = sentence.indexOf(',', lastCommaIndex);
    String temp = sentence.substring(lastCommaIndex, commaIndex);
    values[i] = temp;
    lastCommaIndex = commaIndex + 1;
  }

  // Store latitude and longitude in global networkinfo object
  networkinfo.lat = values[1];
  networkinfo.lon = values[2];
  Serial.println("Latitude(unverified) : " + networkinfo.lat);
  Serial.println("Longitude(unverified) : " + networkinfo.lon);
  SerialMon.println("\n----------   End of readLocation()   ----------\n");
}

void readcellinfo()
{
  SerialMon.println("\n----------   Start of readcellinfo()   ----------\n");
  // Send AT command to retrieve cell info
  String info = sendAT("AT+CPSI?", 10000, 1);

  // Process the response
  int startIndex = info.indexOf("+CPSI: ");
  info = info.substring(startIndex + 7, startIndex + 80);
  info.replace("\r", "");
  info.replace("\n", "");
  info.replace("AT+CLBS=4ERROR", "");
  info.replace("AT+CPSI?+CPSI: NO SERVICE", "");

  // Extract the values from the response
  int commaIndex = 0;
  int lastCommaIndex = 0;
  String values[15];
  for (int i = 0; i < 15; i++)
  {
    commaIndex = info.indexOf(',', lastCommaIndex);
    String temp = info.substring(lastCommaIndex, commaIndex);
    values[i] = temp;
    lastCommaIndex = commaIndex + 1;
  }

  // Convert and store the relevant values in the networkinfo struct
  int lacDec = (int)strtol(values[3].c_str(), NULL, 16);
  networkinfo.rssnr = values[13].toInt();
  networkinfo.mcc = values[2].substring(0, 3);
  networkinfo.mnc = values[2].substring(4, 6);
  networkinfo.lac = lacDec;
  networkinfo.cid = values[4];
  delay(1000);
  Serial.println("Base station info");
  Serial.println("RSSNR : " + String(networkinfo.rssnr));
  Serial.println("MCC   : " + networkinfo.mcc);
  Serial.println("MNC   : " + networkinfo.mnc);
  Serial.println("LAC   : " + String(networkinfo.lac));
  Serial.println("Cell id  : " + networkinfo.cid);
  SerialMon.println("\n----------   End of readcellinfo()   ----------\n");
}

void readsensor()
{
  SerialMon.println("\n----------   Start of readsensor()   ----------\n");
  BLEDevice::init("");
  for (uint8_t i = 0; i < mi_num; i++)
  {
    String mac = milist[i].c_str();
    connectToSensor(BLEAddress(milist[i].c_str()));
    delay(1000);
    tempandhumi.temp[i] = sensordata.temp;
    tempandhumi.humi[i] = sensordata.humi;
    Serial.println("Sensor MAC : " + milist[i] + ", Temperature : " + tempandhumi.temp[i] + " C., Humidity : " + tempandhumi.humi[i] + "%.");
    sensordata.temp = "";
    sensordata.humi = "";
  }
  SerialMon.println("\n----------   End of readsensor()   ----------\n");
}

String createjson()
{
  SerialMon.println("\n----------   Start of createjson()   ----------\n");
  // Create JSON object
  StaticJsonDocument<512> doc;

  doc["ID"] = device_id;
  doc["Date"] = rtc_info.date;
  doc["Time"] = rtc_info.time;
  doc["Offset"] = rtc_info.offset;
  doc["Batt_Lev"] = battinfo.batt_level;
  doc["Vol_Lvl"] = battinfo.batt_volt;
  doc["Lat"] = networkinfo.lat;
  doc["Lon"] = networkinfo.lon;
  for (uint8_t i = 0; i < mi_num; i++)
  {
    String mac = milist[i].c_str();
    doc[mac]["Temp"] = tempandhumi.temp[i];
    doc[mac]["Humi"] = tempandhumi.humi[i];
  }
  doc["MCC"] = networkinfo.mcc;
  doc["MNC"] = networkinfo.mnc;
  doc["LAC"] = networkinfo.lac;
  doc["SCellID"] = networkinfo.cid;

  // Serialize JSON to string
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println("JSON String : " + jsonString);
  SerialMon.println("\n----------   End of createjson()   ----------\n");
  return jsonString;
}

void readLog(String filename)
{
  SerialMon.println("\n----------   Start of readLog()   ----------\n");
  // Mount the SPIFFS file system
  if (!SPIFFS.begin(true))
  {
    SerialMon.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Open the specified file for reading
  File fileRead = SPIFFS.open(filename, "r");

  // Print a message to indicate that the file content is about to be printed
  SerialMon.println("File Content : \n");

  // Read and print the contents of the file, one character at a time
  while (fileRead.available())
  {
    SerialMon.write(fileRead.read());
  }

  // Close the file
  fileRead.close();

  // Unmount the SPIFFS file system
  if (SPIFFS.begin())
  {
    SPIFFS.end();
  }
  SerialMon.println("\n----------   End of readLog()   ----------\n");
}

// This function deletes a file from the SPIFFS file system, given its filename
bool deletelog(String filename)
{
  // Mount the SPIFFS file system
  if (!SPIFFS.begin(true))
  {
    SerialMon.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  // Print the list of files before deleting the specified file
  SerialMon.print(F("Before delete, there are:\r\n"));
  listAllFile(); // assuming this is a function that lists all files in the SPIFFS

  // If the specified file exists, delete it
  if (SPIFFS.exists(filename))
    SPIFFS.remove(filename);

  // Print the list of files after deleting the specified file
  SerialMon.print(F("Afters delete, there are:\r\n"));
  listAllFile(); // assuming this is a function that lists all files in the SPIFFS

  // Unmount the SPIFFS file system and return true to indicate success
  SPIFFS.end();
  return true;
}

void sleep(float sec)
{
  SerialMon.println("\n----------   Start of sleep()   ----------\n");
  double min_d = sec / 60;
  // Set wakeup time to 10 minutes
  esp_sleep_enable_timer_wakeup((sleepmin - min_d) * 60 * 1000000);

  // Print the duration in minutes to the serial monitor
  Serial.print("Duration : ");
  Serial.print(sec / 60);
  Serial.println(" minutes");
  // Go to sleep now
  Serial.print("Going to sleep for ");
  Serial.print((sleepmin - min_d));
  Serial.println(" minutes");
  SerialMon.println("\n----------   End of sleep()   ----------\n");
  esp_deep_sleep_start();
}

void moduleOff()
{
  String _res_shutdown = sendAT("AT+CPOF", 9000, 0);
  digitalWrite(12, LOW);
  Serial.println("_res_shutdown = " + _res_shutdown);
}

void modulePowerOff()
{
  digitalWrite(4, HIGH);
  delay(3000);
  digitalWrite(4, LOW);
  delay(3000);
  digitalWrite(4, HIGH);
  digitalWrite(12, LOW);
}

void sendrequest()
{
  SerialMon.println("\n----------   Start of sendrequest()   ----------\n");
  String payload = "{\"token\":\"" + apiKey + "\",\"radio\":\"lte\",\"mcc\":" + networkinfo.mcc + ",\"mnc\":" + networkinfo.mnc + ",\"cells\":[{\"lac\":" + networkinfo.lac + ",\"cid\":" + networkinfo.cid + ",\"psc\":0}],\"address\":1}";
  String response;
  client.connect("ap1.unwiredlabs.com", 80);
  String request = "POST /v2/process.php HTTP/1.1\r\n";
  request += "Host: ap1.unwiredlabs.com\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: ";
  request += String(payload.length());
  request += "\r\n\r\n";
  request += payload;
  client.print(request);
  while (client.connected())
  {
    while (client.available())
    {
      char c = client.read();
      response += c;
      client.write(c);
    }
  }
  client.stop();
  Serial.println("Response =" + response);
  int startIndex = response.indexOf("\"lat\":");
  int endIndex = response.indexOf(",\"lon\":");
  String lat = response.substring(startIndex + 6, endIndex);
  startIndex = endIndex + 7;
  endIndex = response.indexOf(",\"accuracy\":");
  String lon = response.substring(startIndex, endIndex);
  networkinfo.lat = lat;
  networkinfo.lon = lon;
  Serial.println("Latitude: " + lat);
  Serial.println("Longitude: " + lon);
  SerialMon.println("\n----------   End of sendrequest()   ----------\n");
}

bool upload2FTP(char *_FTPS_ADDR, uint16_t _FTPS_PRT, char *_FTPS_USRN, char *_FTPS_PASS, uint8_t _FTPS_TYPE, char *_FTPS_LOG_PATH, String _filename)
{
  SerialMon.println("\n----------   Start of upload2FTP()   ----------\n");
  char ATcommand[60];

  // 1. Set the current directory to C:
  sendAT("AT+FSCD=C:", 5000, 1);

  // 2. Mount SPIFFS and check if the file exists
  if (!SPIFFS.begin(true))
  {
    SerialMon.println("An error has occurred while mounting SPIFFS.");
    return false;
  }

  if (!SPIFFS.exists(_filename))
  {
    SerialMon.println(String(_filename) + " does not exist in SPIFFS.");
    return false;
  }

  // 3. Open the file and read its size
  File fileToRead = SPIFFS.open(_filename, "r");
  if (!fileToRead)
  {
    SerialMon.printf("Failed to open file for reading. \r\n");
    return false;
  }

  int file_len = fileToRead.size();
  SerialMon.println("File size : " + String(file_len) + " bytes.");
  if (file_len <= 0)
  {
    SerialMon.print("No data in ");
    return false;
  }

  // 4. Transfer the file to the SIM7600
  memset(ATcommand, 0, sizeof(ATcommand));
  sprintf(ATcommand, "AT+CFTRANRX=\"C:%s\",%d", _filename.c_str(), file_len);
  String writefile_response = sendAT(ATcommand, 5000, 1);
  SerialMon.println("Write file response: " + writefile_response);

  while (fileToRead.available())
  {
    if (SerialAT.availableForWrite())
    {
      String data = fileToRead.readString();
      SerialAT.printf("%s", data.c_str());
      SerialMon.println("Sent data: " + data);
    }
    delay(100);
  }

  fileToRead.close();
  SerialAT.flush();
  delay(5000);
  SerialMon.printf("Write file done.\r\n");
  SerialAT.flush();
  delay(5000);
  SerialMon.printf("Write file into sim7600 done.\r\n");

  // 5. Stop all previous FTPS sessions and start a new one
  sendAT("AT+CFTPSLOGOUT", 2000, 1);
  sendAT("AT+CFTPSSTOP", 100, 1);
  delay(5000);
  sendAT("AT+CFTPSSTART", 500, 1);

  // 6. Login to the FTPS server
  memset(ATcommand, 0, sizeof(ATcommand));
  sprintf(ATcommand, "AT+CFTPSLOGIN=\"%s\",%d,\"%s\",\"%s\",%d", _FTPS_ADDR, _FTPS_PRT, _FTPS_USRN, _FTPS_PASS, _FTPS_TYPE);
  sendAT(ATcommand, 6000, 1);

  // 7. Upload file
  String filenameno = _filename;
  filenameno.replace("/", "");
  sprintf(ATcommand, "AT+CFTPSPUTFILE=\"%s\",1", (String(_FTPS_LOG_PATH) + filenameno).c_str());
  String response_uploadfile = sendAT(ATcommand, 20000, 1);
  SerialAT.flush();

  if (response_uploadfile.indexOf("+CFTPSPUTFILE: 0") != -1)
  {
    Serial.println("File transfer was successful");
  }
  else
  {
    Serial.println("File transfer failed");
  }

  // 8. Log out FTPS
  sendAT("AT+CFTPSLOGOUT", 3000, 1);

  // 9. Stop FTPS
  sendAT("AT+CFTPSSTOP", 500, 1);

  // 10. Remove the uploaded file from drive E:
  memset(ATcommand, 0, sizeof(ATcommand));
  sprintf(ATcommand, "AT+FSDEL=%s", _filename.c_str());
  sendAT(ATcommand, 2000, 1);
  SerialMon.println("\n----------   End of upload2FTP()   ----------\n");
  return true;
}

void getRTC()
{
  // Print a message indicating that the function has been called
  SerialMon.println("\n----------   Start of getRTC()   ----------\n");

  // Send some AT commands to the modem to configure time zone settings
  sendAT("AT+CTZU=1", 2000, 1);

  // Request the current time and date from the modem using the AT+CCLK? command
  String timestring = sendAT("AT+CCLK?", 3000, 1);

  // Remove the "+CCLK:" prefix from the modem response
  timestring.replace("+CCLK:", "");

  // Extract the date from the timestring
  int dateStartIndex = timestring.indexOf('"') + 1;
  int dateEndIndex = timestring.indexOf(',');
  rtc_info.date = timestring.substring(dateStartIndex, dateEndIndex);

  // Extract the time and time zone offset from the timestring
  int timeStartIndex = dateEndIndex + 1;
  if (timestring.indexOf('+') != -1)
  {
    // If the time zone offset is positive, extract the time and offset
    int timeEndIndex = timestring.indexOf(':') + 6;
    rtc_info.time = timestring.substring(timeStartIndex, timeEndIndex);
    int offsetStartIndex = timeEndIndex + 1;
    String offset_str = timestring.substring(offsetStartIndex, offsetStartIndex + 2);
    rtc_info.offset = "+" + String(offset_str.toInt() / 4);
  }
  else
  {
    // If the time zone offset is negative, extract the time and offset
    int timeEndIndex = timestring.indexOf('-');
    rtc_info.time = timestring.substring(timeStartIndex, timeEndIndex);
    int offsetStartIndex = timeEndIndex + 1;
    String offset_str = timestring.substring(offsetStartIndex, offsetStartIndex + 2);
    rtc_info.offset = "-" + String(offset_str.toInt() / 4);
  }

  // Print the extracted date, time, and time zone offset to the serial monitor
  Serial.println("Date : " + rtc_info.date);
  Serial.println("Time : " + rtc_info.time);
  Serial.println("Offset : " + rtc_info.offset);
  SerialMon.println("\n----------   End of getRTC()   ----------\n");
}

String createfilename()
{
  SerialMon.println("\n----------   Start of createfilename()   ----------\n");
  // Get the current date from the RTC object and store it in a string variable.
  String datename = rtc_info.date;
  SerialMon.println("Date : " + datename);
  // Remove any forward slashes from the date string.
  datename.replace("/", "");
  // Combine the modified date string with a file extension to create a filename.
  String makefilename = "/" + datename + ".txt";
  // Print the filename to the serial monitor for debugging purposes.
  SerialMon.println("File name : " + makefilename);
  SerialMon.println("\n----------   End of createfilename()   ----------\n");
  return makefilename;
}

// Start of setup function
void setup()
{
  // Start timer to measure program execution time
  unsigned long startTime = millis();

  // Initialize serial communication with two monitors
  SerialMon.begin(115200);                               // Monitor for debugging information
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX); // Monitor for modem AT commands

  // Set pin mode for battery ADC (analog to digital converter)
  pinMode(BAT_ADC, INPUT);

  // Turn on modem and send AT commands
  modemPowerOn();

  // Wait for modem to stabilize
  delay(1000);

  // Connect to LTE network and get RTC (real-time clock) time
  connect2LTE();
  getRTC();

  // Read battery level and location information
  delay(1000);
  readBattLevel();
  delay(500);
  readLocation();

  // Read cell information and data from connected sensor
  delay(2000);
  readcellinfo();
  delay(2000);
  readsensor();
  delay(2000);

  // Create JSON string with all device information and write to log file
  // sendrequest();
  String content = createjson();
  delay(3000);
  Filename = createfilename();
  writelog(Filename, content);

  // Read log file and upload to FTP (File Transfer Protocol) server
  delay(1000);
  readLog(Filename);
  delay(2000);
  upload2FTP(FTPS_ADDR, FTPS_PRT, FTPS_USRN, FTPS_PASS, FTPS_TYPE, FTPS_PATH, Filename);
  delay(2000);

  // Power off modem and prepare for sleep mode
  modulePowerOff();

  // Calculate program execution duration and convert to seconds
  unsigned long endTime = millis();
  unsigned long duration = endTime - startTime;
  float durationSeconds = duration / 1000.0;

  // Put the device to sleep for the calculated duration
  sleep(durationSeconds);
}

void loop()
{
  delay(1000);
}
