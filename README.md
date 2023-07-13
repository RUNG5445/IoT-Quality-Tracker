# Quality tracking System with IoT and FTP Integration

This project uses T-sim7600g-h 4G LTE Cat4 Module and ESP32-WROVER-B WiFi module to read humidity and temperature values from a sensor. It retrieves location data based on the cell ID via an API. The collected information is logged and sent to an FTP server for storage and analysis.

## Introduction

This project utilizes the T-sim7600g-h 4G LTE Cat4 Module and ESP32-WROVER-B WiFi module to read humidity and temperature values from an external sensor. It also retrieves the latitude and longitude from an API based on the sent cell ID. The gathered information is then used to create logs and sent to an FTP server via FTP.

## Features

- Reads humidity and temperature values from an external sensor
- Retrieves latitude and longitude from an API using the cell ID
- Creates logs based on the collected information
- Sends logs to an FTP server via FTP protocol

## Requirements

- T-sim7600g-h 4G LTE Cat4 Module
- ESP32-WROVER-B WiFi module
- External humidity and temperature sensor
- Internet connectivity

## Usage

1. Connect the T-sim7600g-h module to the microcontroller for internet connectivity.
2. Connect the external humidity and temperature sensor to the microcontroller.
3. Configure the WiFi settings on the T-sim7600g-h module if required.
4. Set up the API to retrieve the latitude and longitude based on the cell ID.
5. Run the code to read the humidity and temperature values from the sensor and retrieve the latitude and longitude from the API.
6. Create logs based on the collected data.
7. Send the logs to the configured FTP server for storage and analysis.
