import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32MultiArray
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from tf2_ros import TransformBroadcaster
import math

class EncoderOdometryPublisher(Node):
    def __init__(self):
        super().__init__('encoder_odometry_publisher')

        # Physical Robot Calibration Constants
        self.TICKS_PER_REV = 692.0
        self.WHEEL_RADIUS = 0.017   # 1.7 cm
        self.WHEEL_BASE = 0.159     # 15.9 cm

        # Robot Pose State Variables
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0

        # Latest raw ticks received from ESP32
        self.latest_left_ticks = None
        self.latest_right_ticks = None
        self.prev_left_ticks = None
        self.prev_right_ticks = None
        self.last_time = self.get_clock().now()

        # Subscriber & Publisher
        self.sub = self.create_subscription(Int32MultiArray, 'encoder_ticks', self.encoder_callback, 10)
        self.odom_pub = self.create_publisher(Odometry, 'odom', 10)
        self.tf_broadcaster = TransformBroadcaster(self)

        # Continuous Timer (20 Hz) to keep the TF tree alive
        self.timer = self.create_timer(0.05, self.update_and_publish_odometry)
        self.get_logger().info("Encoder Odometry Node Initialized")

    def encoder_callback(self, msg):
        if len(msg.data) >= 2:
            self.latest_left_ticks = msg.data[0]
            self.latest_right_ticks = msg.data[1]

    def update_and_publish_odometry(self):
        if self.latest_left_ticks is None or self.latest_right_ticks is None:
            return

        current_time = self.get_clock().now()
        dt = (current_time - self.last_time).nanoseconds / 1e9

        if self.prev_left_ticks is None:
            self.prev_left_ticks = self.latest_left_ticks
            self.prev_right_ticks = self.latest_right_ticks
            self.last_time = current_time
            return

        d_left_ticks = self.latest_left_ticks - self.prev_left_ticks
        d_right_ticks = self.latest_right_ticks - self.prev_right_ticks
        self.prev_left_ticks = self.latest_left_ticks
        self.prev_right_ticks = self.latest_right_ticks
        self.last_time = current_time

        meters_per_tick = (2.0 * math.pi * self.WHEEL_RADIUS) / self.TICKS_PER_REV
        d_left = d_left_ticks * meters_per_tick
        d_right = d_right_ticks * meters_per_tick

        d_center = (d_left + d_right) / 2.0
        d_theta = (d_right - d_left) / self.WHEEL_BASE

        self.x += d_center * math.cos(self.theta + (d_theta / 2.0))
        self.y += d_center * math.sin(self.theta + (d_theta / 2.0))
        self.theta += d_theta

        qz = math.sin(self.theta / 2.0)
        qw = math.cos(self.theta / 2.0)

        # Broadcast TF Transform
        t = TransformStamped()
        t.header.stamp = current_time.to_msg()
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_footprint'
        t.transform.translation.x = self.x
        t.transform.translation.y = self.y
        t.transform.translation.z = 0.0
        t.transform.rotation.z = qz
        t.transform.rotation.w = qw
        self.tf_broadcaster.sendTransform(t)

        # Publish Odometry
        odom = Odometry()
        odom.header.stamp = current_time.to_msg()
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_footprint'
        odom.pose.pose.position.x = self.x
        odom.pose.pose.position.y = self.y
        odom.pose.pose.orientation.z = qz
        odom.pose.pose.orientation.w = qw

        if dt > 0:
            odom.twist.twist.linear.x = d_center / dt
            odom.twist.twist.angular.z = d_theta / dt

        self.odom_pub.publish(odom)

def main(args=None):
    rclpy.init(args=args)
    node = EncoderOdometryPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
