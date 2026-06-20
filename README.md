# Smart Energy Meter Smart Plug

This project is a portable ESP32-based Smart Energy Meter and Smart Plug system.

## Main Features

* Real-time voltage, current, power, energy, and cost monitoring
* OLED display for live values
* Mobile app monitoring and control using Flutter
* Firebase Realtime Database connection
* Relay ON/OFF control from app and physical button
* Wi-Fi setup mode for new users
* Energy history and analysis
* Timer-based auto OFF function
* Power-cut detection and auto relay handling

## Hardware Used

* ESP32 development board
* ACS712 current sensor
* ZMPT101B voltage sensor
* OLED display
* Relay module
* Buzzer
* Two push buttons
* 5V power supply module
* Universal socket and plug enclosure

## Folder Structure

* `Arduino/` - ESP32 Arduino firmware
* `Flutter_App/` - Flutter mobile application
* `Documentation/` - PDF guide and notes
* `Images/` - Product images and screenshots

## Safety Note

This project works with AC mains voltage. Proper insulation, fuse protection, enclosure, and safe wiring are required. Do not touch live AC parts while powered.
