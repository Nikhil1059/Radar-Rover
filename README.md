# Radar-Rover
It is a Rover with various sensors on board and can also be driven manually. It can go to places where it is hard to reach for humans and scan its surrounding and create a 2D Map.
This project stands out as I use time of flight sensor to make a pseudo liDAR. I mount tthe TOF sensor so a servo rig to pan and tilt it so that it can scan the sorroundings and make the 2d map.
The rover also contains a acclerometer and encoders for the motors which help in makinf the mapping process acurate.

---

The Devlogs is the work I have done which I have documented after each modification or the time I worked on this project.
If you feel like you would want to know the journey of how this project is being made please be free to check them out.

---

### The circuit connections:

 **Power Mapping**
* Buck Converter OUT (+) -> DRV8833 VCC & EEP | Servos VCC (5V Rail)
* ESP32 3V3 Pin -> MPU6050 VCC | ToF VCC | Encoder VCC (3.3V Rail)
* Common Connection -> All GND Pins Linked (Universal GND)

 **DRV8833 Motor Driver**
* DRV8833 IN1 -> ESP32 GPIO 14
* DRV8833 IN2 -> ESP32 GPIO 27
* DRV8833 IN3 -> ESP32 GPIO 16
* DRV8833 IN4 -> ESP32 GPIO 17
* DRV8833 OUT1 -> Left Motor M1 (White Wire)
* DRV8833 OUT2 -> Left Motor M2 (Red Wire)
* DRV8833 OUT3 -> Right Motor M1 (White Wire)
* DRV8833 OUT4 -> Right Motor M2 (Red Wire)

 **Shared I2C Sensor Bus**
* MPU6050 SDA -> ESP32 GPIO 21
* MPU6050 SCL -> ESP32 GPIO 22
* ToF Sensor SDA -> ESP32 GPIO 21
* ToF Sensor SCL -> ESP32 GPIO 22
* ToF Sensor XSHUT -> ESP32 GPIO 4

 **Quadrature Encoders**
* Left Motor C1 (Green) -> ESP32 GPIO 34
* Left Motor C2 (Yellow) -> ESP32 GPIO 35
* Right Motor C1 (Green) -> ESP32 GPIO 32
* Right Motor C2 (Yellow) -> ESP32 GPIO 33

 **SG90 Pan-Tilt Servos**
* Pan Servo Signal -> ESP32 GPIO 13
* Tilt Servo Signal -> ESP32 GPIO 25
