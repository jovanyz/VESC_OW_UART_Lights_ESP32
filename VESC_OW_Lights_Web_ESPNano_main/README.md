Wireless control of NeoPixels using a web graphical interface running on an Arduino.

Designed for the Arduino Nano ESP32.

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Huge thanks to  <http://www.penguintutor.com/projects/arduino-rp2040-pixelstrip>  for creating the base code for this project.

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Default WiFi name is "myVESC", and default password is "yourpassword". Once connected, visit the web address: http://192.168.4.1, you can create an app to redirect to this page on your phone.

Personal LED parameters should be set in config.h

- As is, it uses the Arduino Nano ESP32 and NeoPixels (separate code included for RGBW). 
- Creates it own WiFi network, which you conect to, and it hosts a webpage page at: 192.168.4.1 . This webpage allows you to control the lights. 
- Coded for a UART connection from the VESC to Arduino, and requires the VescUart library (https://github.com/SolidGeek/VescUart). The get data setting is disabled by default, so you can choose to leave UART open for other stuff, but you will lose the directional reaction.
- OTA mode will allow the device to be updated wirelessly via the Arduino IDE or similar. Be sure you fill out your home WiFi details to use this feature.
- BLE bridge mode will use the UART port to create a new VESC connectable bluetooth device and provides a more stable connection, especially on iOS. Speed reactive light modes will be paused when this is active.
- If UART is connected the lights will react to the VESC direction and speed. Other parameters (voltage,  Amp h, etc ) can also be retrieved but this is not currently implemented.  
- You should fill out all the const variables in "config.h" to make sure your lights run correctly.  
- Data wire from the ardunio goes into headlights first, then tail lights-- all connected to one Arduino pin. 
- Wiring from the Focer to Arduino is (5V -> Vin), (Gnd  ->  Gnd), and (RX -> TX), (TX -> RX) if using UART.
