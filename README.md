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

### How It Works & Getting It Running:
To get this rover mapping rooms, we bridge an ESP32 microcontroller running micro-ROS over Wi-Fi directly to a Linux Mint laptop running ROS 2. Instead of spending a fortune on a real LiDAR, the ESP32 handles a low-level multitasking loop. It reads wheel encoders via hardware interrupts, controls the DRV8833 motor driver, sweeps an SG90 servo holding a VL53L0X Time-of-Flight (ToF) laser sensor across a 180 degrees, and streams everything over UDP packets to the laptop.   

On the laptop side, a Python node calculates wheel odometry from raw encoder ticks, a static transform publisher links the physical chassis to the laser, and SLAM Toolbox stitches the sweeping laser arcs together with wheel movement to paint a live 2D floor plan in RViz2.   

Follow these step-by-step commands to boot up the entire stack and start mapping your room:

**Step 1: Flash the ESP32 Firmware**
 * Open your project in VS Code/PlatformIO.
 * Ensure your platformio.ini uses a stable framework configuration and includes the micro-ROS platformio library.
 *Update your Wi-Fi credentials (ssid, password) and your laptop's local IP address (agent_ip) inside src/main.cpp.
 * Build and upload the code to your ESP32

 **Step 2: The 5-Terminal Master Execution Loop**

 Terminal 1: Start the micro-ROS Agent
  This acts as the network bridge, catching the UDP Wi-Fi data packets from your ESP32 and injecting them straight into ROS 2:
     
       ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888 

 Terminal 2: Launch the Wheel Odometry Node
  Navigate to your project directory where your Python odometry script lives:  

       python3 odom_publisher.py 

 Terminal 3: Publish the Static Sensor Transform
  Tell ROS 2 exactly where your sweeping ToF sensor is mounted relative to the center of the wheel axle
  (11.8cm forward & 8cm up): 

      ros2 run tf2_ros static_transform_publisher --x 0.118 --y 0.0 --z 0.08 --yaw 0 --pitch 0 --roll 0 --frame-id base_footprint --child-frame-id  laser_frame

Terminal 4: Launch SLAM Toolbox
  Boot up the 2D mapping engine to start processing your sensor data into an occupancy grid map:

    ros2 launch slam_toolbox online_async_launch.py use_sim_time:=false

Terminal 5: Keyboard Teleoperation
  Run the native ROS 2 teleop node to drive your rover manually using your laptop keyboard:

    ros2 run teleop_twist_keyboard teleop_twist_keyboard


**Step 3: Visualize and Map in RViz2**
Open a 6th terminal window to boot up the lightweight 3D visualizer:
     ```rviz```

Once RViz2 opens on your screen, configure these quick settings on the left sidebar:

 1. Fixed Frame: Change map to odom initially (switch to map once SLAM Toolbox initializes its grid)
 2. Add Laser Scan (/scan):
     -> Click Add (bottom left) $\rightarrow$ Select the By topic tab $\rightarrow$ Double-click /scan (LaserScan).  
     -> Expand the LaserScan menu
     -> Change Reliability Policy to Best Effort (Mandatory for micro-ROS Wi-Fi streams).
     -> Set Size (m) to 0.05 so the laser points are bold and clear.
     -> Change Color Transformer to FlatColor and pick a bright color like red or green.
 3.  Add Map (/map):
     -> Click Add - By topic tab - Double-click /map (Map).
     -> Under Map properties, ensure Durability Policy is set to Transient Local. (Pro-tip: You can uncheck this map box if you just want to see clean            red laser borders drawing your room perimeter in real time!)
 4. Add Robot TF:
    -> Click Add - By display type tab - Double-click TF to see your coordinate frames moving. 

Now, use your keyboard control terminal (i, j, l, k) to drive the rover around your room. Watch as your Pseudo-LiDAR sweep combines with wheel odometry to paint an accurate, real-time blueprint of your walls and furniture right on your screen!

     

      
  

