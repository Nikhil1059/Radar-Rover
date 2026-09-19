#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <micro_ros_platformio.h>

#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <geometry_msgs/msg/twist.h>
#include <std_msgs/msg/int32_multi_array.h>

#include <VL53L0X.h>
#include <sensor_msgs/msg/laser_scan.h>

// Wifi credentials and configuration
char ssid[] = "Airtel_kama_4438";
char password[] = "70329kamal14438";
IPAddress agent_ip(192, 168, 1, 13);
size_t agent_port = 8888;

// Motor pins (DRV8833)
const int IN1 = 14; 
const int IN2 = 27; 
const int IN3 = 19; 
const int IN4 = 23; 

// Encoder pins
const int LEFT_ENC_A = 34;
const int LEFT_ENC_B = 35;
const int RIGHT_ENC_A = 32;
const int RIGHT_ENC_B = 33;

// Encoder Tick counters
volatile int32_t left_encoder_ticks = 0;
volatile int32_t right_encoder_ticks = 0;

// Pan Tilt Pins
const int PAN_SERVO_PIN = 13;
const int TILT_SERVO_PIN = 25;
const int PAN_PWM_CHANNEL = 0;
const int TILT_PWM_CHANNEL = 1;

// Physical Mechanical Calibration
const int TILT_LEVEL_ANGLE = 150; // Perfectly straight parallel to the floor

// Scan Array Config
#define NUM_SAMPLES 37 
#define STEP_DEG 5.0

// Hardare objects
VL53L0X tofSensor;

// micro ros global handles
rcl_subscription_t cmd_sub;
geometry_msgs__msg__Twist twist_msg;

// micro ros encoder publisher handles
rcl_publisher_t enc_pub;
std_msgs__msg__Int32MultiArray enc_msg;
int32_t enc_buffer[2]; 

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

// micro-ROS Scan Message handler
rcl_publisher_t scan_pub;
sensor_msgs__msg__LaserScan scan_msg;
float range_buffer[NUM_SAMPLES]; 

// Sweep State Machine Variables
int current_sample_idx = 0;
int current_pan_angle = 0;
int sweep_dir = 1; 

// Helper: Servo control via esp32 PWM
void writeServoAngle(int channel, int angle_deg) {
    int pulse_us = map(angle_deg, 0, 180, 500, 2500);
    uint32_t duty = map(pulse_us, 0, 20000, 0, 16383);
    ledcWrite(channel, duty);
}

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){while(1){delay(100);}}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

void IRAM_ATTR leftEncoderISR(){
    if (digitalRead(LEFT_ENC_B) == HIGH) {
        left_encoder_ticks--;  
    } else {
        left_encoder_ticks++;
    }
}

void IRAM_ATTR rightEncoderISR() {
    if(digitalRead(RIGHT_ENC_B) == HIGH) {
        right_encoder_ticks++;
    } else {
        right_encoder_ticks--;
    }
}

void cmd_vel_callback(const void * msin) {
    const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msin;
    float linear_x = msg->linear.x;
    float angular_z = msg->angular.z;

    float left_speed = linear_x - angular_z;
    float right_speed = linear_x + angular_z;

    int left_pwm = constrain(fabs(left_speed) * 255.0, 0, 255);
    int right_pwm = constrain(fabs(right_speed) * 255.0, 0, 255);

    if(left_speed > 0){
        analogWrite(IN1, left_pwm); analogWrite(IN2, 0);
    } else if (left_speed < 0) {
        analogWrite(IN1, 0);        analogWrite(IN2, left_pwm);
    } else {
        analogWrite(IN1, 0);        analogWrite(IN2, 0);
    }

    if(right_speed > 0) {
        analogWrite(IN3, right_pwm); analogWrite(IN4, 0);
    } else if (right_speed < 0) {
        analogWrite(IN3, 0);         analogWrite(IN4, right_pwm);
    } else {
        analogWrite(IN3, 0);         analogWrite(IN4, 0);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT); digitalWrite(IN1, LOW);
    pinMode(IN2, OUTPUT); digitalWrite(IN2, LOW);
    pinMode(IN3, OUTPUT); digitalWrite(IN3, LOW);
    pinMode(IN4, OUTPUT); digitalWrite(IN4, LOW);

    set_microros_wifi_transports(ssid, password, agent_ip, agent_port);
    delay(1000);

    allocator = rcl_get_default_allocator();
    RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
    RCCHECK(rclc_node_init_default(&node, "radar_rover_node", "", &support));

    RCCHECK(rclc_subscription_init_default(
        &cmd_sub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "cmd_vel"
    ));

    RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
    RCCHECK(rclc_executor_add_subscription(&executor, &cmd_sub, &twist_msg, &cmd_vel_callback, ON_NEW_DATA));

    pinMode(LEFT_ENC_A, INPUT); pinMode(LEFT_ENC_B, INPUT);
    pinMode(RIGHT_ENC_A, INPUT); pinMode(RIGHT_ENC_B, INPUT);

    attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, RISING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, RISING);

    RCCHECK(rclc_publisher_init_default(
        &enc_pub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32MultiArray),
        "encoder_ticks"
    ));

    enc_msg.data.data = enc_buffer;
    enc_msg.data.size = 2;
    enc_msg.data.capacity = 2;

    ledcSetup(PAN_PWM_CHANNEL, 50, 14); 
    ledcAttachPin(PAN_SERVO_PIN, PAN_PWM_CHANNEL);

    ledcSetup(TILT_PWM_CHANNEL, 50, 14);
    ledcAttachPin(TILT_SERVO_PIN, TILT_PWM_CHANNEL);

    // Park pan at center and lock tilt perfectly straight
    writeServoAngle(PAN_PWM_CHANNEL, 90);
    writeServoAngle(TILT_PWM_CHANNEL, TILT_LEVEL_ANGLE);

    Wire.begin(); 
    tofSensor.setTimeout(500);
    if (tofSensor.init()) {
        tofSensor.startContinuous(20); 
    }

    RCCHECK(rclc_publisher_init_default(
        &scan_pub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, LaserScan),
        "scan"
    ));

    scan_msg.header.frame_id.data = (char*)"laser_frame";
    scan_msg.header.frame_id.size = strlen("laser_frame");
    scan_msg.header.frame_id.capacity = scan_msg.header.frame_id.size + 1;

    scan_msg.angle_min = -M_PI / 2.0;                             
    scan_msg.angle_max = M_PI / 2.0;                            
    scan_msg.angle_increment = (STEP_DEG * M_PI) / 180.0; 
    scan_msg.time_increment = 0.02;                       
    scan_msg.scan_time = 37 * 0.02;                    
    scan_msg.range_min = 0.03;                            
    scan_msg.range_max = 2.00;                            
    
    scan_msg.ranges.data = range_buffer;
    scan_msg.ranges.size = NUM_SAMPLES;
    scan_msg.ranges.capacity = NUM_SAMPLES;

    rmw_uros_sync_session(1000);
}

void loop() {
    RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
    delay(10);

    // Re-synchronize the clock every 10 seconds to prevent RViz2 "future" errors
    static unsigned long last_time_sync = 0;
    if (millis() - last_time_sync > 10000) {
        rmw_uros_sync_session(100);
        last_time_sync = millis();
    }

    enc_buffer[0] = left_encoder_ticks;
    enc_buffer[1] = right_encoder_ticks;
    RCSOFTCHECK(rcl_publish(&enc_pub, &enc_msg, NULL));

    static unsigned long last_scan_time = 0;

    // Clean, simplified single-pass radar sweep
    if(millis() - last_scan_time >= 25) {
        last_scan_time = millis();
    
        current_pan_angle = current_sample_idx * STEP_DEG;
        writeServoAngle(PAN_PWM_CHANNEL, current_pan_angle);

        float raw_distance_m = tofSensor.readRangeContinuousMillimeters() / 1000.0;
        
        // Directly store the distance
        range_buffer[current_sample_idx] = raw_distance_m;

        current_sample_idx += sweep_dir;

        // Publish scan array when a full 180-degree sweep finishes
        if (current_sample_idx >= NUM_SAMPLES || current_sample_idx < 0) {
            scan_msg.header.stamp.sec = rmw_uros_epoch_millis() / 1000;
            scan_msg.header.stamp.nanosec = (rmw_uros_epoch_millis() % 1000) * 1000000;
            RCSOFTCHECK(rcl_publish(&scan_pub, &scan_msg, NULL));

            sweep_dir *= -1;
            current_sample_idx += sweep_dir;
        }
    }
}
