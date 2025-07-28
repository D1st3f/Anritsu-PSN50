# Anritsu Controller

**Anritsu Controller** is a cross-platform Qt-based desktop application for communicating with Anritsu power sensors over a serial connection. It provides a graphical interface for setting measurement parameters, performing calibrations, and monitoring power and temperature in real time.

---

## 🔧 Features

- Serial port connection to Anritsu devices
- Device identification and firmware version display
- Real-time RF power measurements (dBm and Watts)
- Frequency setting (MHz)
- Attenuation input and correction (dB)
- Start/Stop continuous measurements
- Temperature monitoring
- Zero calibration command
- Command/response logging to text view

---
## Screenshots
![2](https://github.com/user-attachments/assets/c6aa65a5-513f-4a12-badf-20bee805e569)
![3](https://github.com/user-attachments/assets/03bd6d2f-5c0d-4590-87ea-42cb662d6c9d)
---
## 📦 Requirements

- **Qt 5.12+** or **Qt 6.x**
- **C++17** or later
- **Anritsu power sensor** supporting serial communication
- OS: Windows, Linux, or macOS

---

## 🚀 Build Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/anritsu-controller.git
cd anritsu-controller
```
### 2. Build with Qt Creator
- Open anritsu-controller in Qt Creator
- Select your kit and build the project

OR

### 2. Build from Terminal
```bash
qmake
make            # or mingw32-make on Windows
```

## 🧪 How to Use
### 1. Connect Device
 - Use only wits rs232 adapter (I have 2 identical ftdi, but only 1 works, also hl340 work fine too with proper driver) + null modem + 8-18V DC power suply
![20250728_224003](https://github.com/user-attachments/assets/ab14f112-60db-4c64-ac45-872bbc7853cf)
 - Select the appropriate serial port
 - Click Connect

### 2. Configure Measurement
 - Enter the desired frequency (in MHz)
 - Click Set
### 3. Adjust Attenuation
 - Set attenuation in dB if needed
 - Click Set
### 4. Perform Zero Calibration
 - Click Zero to initiate the zeroing procedure
 - Interface will be temporarily disabled during calibration
### 5. Start Measurement
- Click Start to begin periodic power measurements
- Click Stop to pause
### 6. View Logs
 - All commands and responses are shown in the log viewer

