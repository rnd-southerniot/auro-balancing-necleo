"""Balance control node for the Auro self-balancing robot.

Subscribes to /imu/data, runs a complementary-filter pitch estimator and
50 Hz PID balance loop, and publishes /cmd_vel to drive the motors.
Includes a 500 ms IMU watchdog that stops motors on sensor dropout.
"""

import math
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from sensor_msgs.msg import Imu


class BalanceNode(Node):
    """ROS2 node implementing a host-side balance PID controller."""

    # Complementary filter coefficient (0..1). Higher = trust gyro more.
    COMP_FILTER_ALPHA = 0.98

    # IMU watchdog timeout in seconds.
    IMU_WATCHDOG_TIMEOUT = 0.5

    # Control loop period in seconds (50 Hz).
    CONTROL_PERIOD = 0.02

    def __init__(self) -> None:
        super().__init__('balance_node')

        # -- Declare parameters --
        self.declare_parameter('balance_angle_deg', -28.4)
        self.declare_parameter('kp', 0.0)
        self.declare_parameter('ki', 0.0)
        self.declare_parameter('kd', 0.0)

        # -- State --
        self._pitch_deg: float = 0.0
        self._last_imu_time: float = time.monotonic()
        self._last_control_time: float = time.monotonic()

        # PID state
        self._integral: float = 0.0
        self._prev_error: float = 0.0

        # -- Publishers --
        self._cmd_vel_pub = self.create_publisher(Twist, '/cmd_vel', 10)

        # -- Subscribers --
        self._imu_sub = self.create_subscription(
            Imu, '/imu/data', self._imu_callback, 10
        )

        # -- Control timer (50 Hz) --
        self._control_timer = self.create_timer(
            self.CONTROL_PERIOD, self._control_loop
        )

        self.get_logger().info(
            f'Balance node started. '
            f'Setpoint: {self._get_balance_angle():.1f} deg, '
            f'Kp={self._get_kp()}, Ki={self._get_ki()}, Kd={self._get_kd()}'
        )

    # -- Parameter accessors --

    def _get_balance_angle(self) -> float:
        return self.get_parameter('balance_angle_deg').value

    def _get_kp(self) -> float:
        return self.get_parameter('kp').value

    def _get_ki(self) -> float:
        return self.get_parameter('ki').value

    def _get_kd(self) -> float:
        return self.get_parameter('kd').value

    # -- IMU callback --

    def _imu_callback(self, msg: Imu) -> None:
        """Update pitch estimate from IMU data using complementary filter."""
        now = time.monotonic()
        dt = now - self._last_imu_time
        self._last_imu_time = now

        if dt <= 0.0 or dt > 0.1:
            # Skip on first call or if dt is unreasonable
            dt = self.CONTROL_PERIOD

        # Accelerometer pitch (degrees)
        ax = msg.linear_acceleration.x
        az = msg.linear_acceleration.z
        accel_pitch_deg = math.degrees(math.atan2(ax, az))

        # Gyroscope angular velocity around Y axis (deg/s)
        gyro_y_deg_s = math.degrees(msg.angular_velocity.y)

        # Complementary filter
        self._pitch_deg = (
            self.COMP_FILTER_ALPHA * (self._pitch_deg + gyro_y_deg_s * dt)
            + (1.0 - self.COMP_FILTER_ALPHA) * accel_pitch_deg
        )

    # -- Control loop --

    def _control_loop(self) -> None:
        """50 Hz PID balance loop with IMU watchdog."""
        now = time.monotonic()
        dt = now - self._last_control_time
        self._last_control_time = now

        # IMU watchdog: stop motors if no IMU data received recently
        imu_age = now - self._last_imu_time
        if imu_age > self.IMU_WATCHDOG_TIMEOUT:
            self._stop_motors()
            self.get_logger().warn(
                f'IMU watchdog triggered ({imu_age:.2f}s since last IMU)',
                throttle_duration_sec=2.0,
            )
            self._integral = 0.0
            self._prev_error = 0.0
            return

        # PID gains (re-read each cycle for dynamic reconfigure)
        kp = self._get_kp()
        ki = self._get_ki()
        kd = self._get_kd()

        # Skip control if gains are all zero (not yet tuned)
        if kp == 0.0 and ki == 0.0 and kd == 0.0:
            return

        # Error: setpoint - measurement
        setpoint = self._get_balance_angle()
        error = setpoint - self._pitch_deg

        # PID terms
        self._integral += error * dt
        # Anti-windup clamp
        self._integral = max(-50.0, min(50.0, self._integral))

        derivative = (error - self._prev_error) / dt if dt > 0.0 else 0.0
        self._prev_error = error

        output = kp * error + ki * self._integral + kd * derivative

        # Publish as cmd_vel linear.x (balance drives forward/backward)
        cmd = Twist()
        cmd.linear.x = max(-1.0, min(1.0, output))
        self._cmd_vel_pub.publish(cmd)

    def _stop_motors(self) -> None:
        """Publish zero velocity to stop motors."""
        cmd = Twist()
        cmd.linear.x = 0.0
        cmd.angular.z = 0.0
        self._cmd_vel_pub.publish(cmd)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = BalanceNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node._stop_motors()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
