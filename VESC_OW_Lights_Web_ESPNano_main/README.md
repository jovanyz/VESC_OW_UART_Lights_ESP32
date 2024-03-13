Wireless control of NeoPixels using a web graphical interface running on an Arduino.

Designed for the Arduino Nano RP2040 connect.
Needs an Arduino that supports WiFiNINA. 

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Huge thanks to  <http://www.penguintutor.com/projects/arduino-rp2040-pixelstrip>  for creating the base code for this project.

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Default WiFi name is "myVESC", and default password is "yourpassword". Once connected, visit the web address: http://192.168.4.1 .  All website commands will be sent to the LEDs on clicking Apply.

Personal LED parameters should be set in config.h

You must download the VescUart Arduino library for this code to work: https://github.com/SolidGeek/VescUart 



- As is, it uses the Arduino Nano RP2040 Connect and NeoPixels (mine are RGB SK6812s). It should work on other IOT Nanos that use the WiFiNINA library.
- Creates it own WiFi network, which you conect to, and it hosts a webpage page at: 192.168.4.1 . This webpage allows you to control the lights. 
- Coded for a UART connection from the VESC to Arduino, and requires the VescUart library (https://github.com/SolidGeek/VescUart). The get data setting is disabled by default, so you can choose to leave UART open for other stuff, but you will lose the directional reaction. You still have to install the library tho, unless you wanna do some coding. 
-  If UART is connected the lights will react to the VESC direction and speed. Other parameters (voltage,  Amp h, etc ) can also be retrieved but this is not currently implemented.  
- You should fill out all the const variables in "config.h" to make sure your lights run correctly.  
- Data wire from the ardunio goes into headlights first, then tail lights-- all connected to one Arduino pin. 
- Wiring from the Focer to Arduino is (5V -> Vin), (Gnd  ->  Gnd), and (RX -> TX), (TX -> RX) if using UART.
