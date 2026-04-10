from setuptools import find_packages, setup

package_name = 'auro_balance'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/balance.launch.py']),
        ('share/' + package_name + '/config', []),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='robotics',
    maintainer_email='robotics@localhost',
    description='Balance controller and dashboard for the Auro self-balancing robot.',
    license='MIT',
    entry_points={
        'console_scripts': [
            'balance_node = auro_balance.balance_node:main',
        ],
    },
)
