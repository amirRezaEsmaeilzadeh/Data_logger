# Data Logger & ESP32 Edge Node

A robust, fault-tolerant Data Acquisition (DAQ) system featuring a custom ESP32 edge node and a multi-threaded Qt C++ desktop application. This project captures, buffers, and transmits real-time environmental and kinematic telemetry over a TCP Wi-Fi socket.

![Overview Dashboard](Images/image_of_app_overview.png)

## System Architecture

This project is divided into two distinct components operating in tandem:

**1. The ESP32 Edge Node (Hardware & Firmware)**
*   **Sensors:** Interfaces with a DHT11 (Temperature/Humidity) and an ADXL345 (3-Axis Accelerometer).
*   **Memory Management:** Implements a custom RAM buffering strategy to prevent flash memory wear, dumping data to `LittleFS` in chunks.
*   **Circular Storage:** Maintains a self-cleaning 7-day rotating log of `.csv` files stored in non-volatile memory.
*   **Network:** Acts as a dedicated TCP server (Port 8080), handling connection drops and rejecting secondary connections to protect the data stream.

**2. Data Logger (Desktop Application)**
*   **Live Telemetry:** Uses `QCustomPlot` to render oscilloscope-style sweeps of incoming sensor data locked to a 60-second sliding window.
*   **State Management:** Separates Live Data feeds from Historical Data views, calculating real-time maximum, minimum, and average statistics.
*   **Network Handshake:** Dynamically queries the ESP32 for its internal directory to map available offline logs.

## Hardware Setup

The physical prototype is powered by a 9V battery and utilizes standard I2C and GPIO protocols to communicate with the sensors.

*   **MCU:** ESP32-WROOM
*   **Environment:** DHT11 (Pin 4)
*   **Kinematics:** ADXL345 (I2C)

![Circuit Wiring 1](Images/image_of_circuit_1.jpg)

![Circuit Wiring 2](Images/image_of_circuit_2.jpg)

## Software Features

### Live Data Streaming
The desktop client actively separates data streams into dedicated tabs, calculating running statistics on the fly.

* **Environment Tab:** Tracks Temperature and Humidity.
![Live Environment](Images/image_of_app_temp_and_hum.png)

* **Kinematics Tab:** Tracks X, Y, and Z acceleration vectors.
![Live Kinematics](Images/image_of_app_accelerometer.png)

### Offline History Parsing
The software allows users to query the ESP32's onboard storage, download specific historical days, and parse the CSV files into static, analytical views without interrupting the background data collection.

![History Dropdown](Images/image_of_app_history_dropdown.png)

![Historical Data View](Images/image_of_app_history_view.png)

## How to Run

1.  Flash the `Firmware/` code to an ESP32 using the Arduino IDE. Update the Wi-Fi credentials to match your local network.
2.  Open the Serial Monitor to identify the ESP32's local IP address.
3.  Compile and run the Qt project in the `Software/` folder using Qt Creator (requires the `network` and `printsupport` modules).
4.  Enter the ESP32's IP address into the desktop application's connection bar and click **Connect**.
