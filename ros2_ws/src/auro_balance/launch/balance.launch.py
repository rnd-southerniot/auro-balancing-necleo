"""Launch file for the Auro self-balancing robot system.

Starts the micro-ROS agent (serial transport) and the balance control node.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # -- Launch arguments --
    serial_port_arg = DeclareLaunchArgument(
        'serial_port',
        default_value='/dev/ttyACM0',
        description='Serial port for micro-ROS agent'
    )

    baud_rate_arg = DeclareLaunchArgument(
        'baud_rate',
        default_value='921600',
        description='Baud rate for micro-ROS serial transport'
    )

    balance_angle_arg = DeclareLaunchArgument(
        'balance_angle_deg',
        default_value='-28.4',
        description='Target balance pitch angle in degrees'
    )

    kp_arg = DeclareLaunchArgument(
        'kp', default_value='0.0',
        description='Balance PID proportional gain'
    )

    ki_arg = DeclareLaunchArgument(
        'ki', default_value='0.0',
        description='Balance PID integral gain'
    )

    kd_arg = DeclareLaunchArgument(
        'kd', default_value='0.0',
        description='Balance PID derivative gain'
    )

    # -- micro-ROS agent --
    micro_ros_agent = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'micro_ros_agent', 'micro_ros_agent',
            'serial',
            '--dev', LaunchConfiguration('serial_port'),
            '-b', LaunchConfiguration('baud_rate'),
        ],
        name='micro_ros_agent',
        output='screen',
    )

    # -- Balance node --
    balance_node = Node(
        package='auro_balance',
        executable='balance_node',
        name='balance_node',
        output='screen',
        parameters=[{
            'balance_angle_deg': LaunchConfiguration('balance_angle_deg'),
            'kp': LaunchConfiguration('kp'),
            'ki': LaunchConfiguration('ki'),
            'kd': LaunchConfiguration('kd'),
        }],
    )

    return LaunchDescription([
        serial_port_arg,
        baud_rate_arg,
        balance_angle_arg,
        kp_arg,
        ki_arg,
        kd_arg,
        micro_ros_agent,
        balance_node,
    ])
