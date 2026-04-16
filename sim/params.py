"""
params.py — physical parameters from STEP extraction + measurements
Updated: SIM-02 session

Sources:
  Mass:     1000g kitchen scale
  Wheel:    65mm diameter calipers
  l_com:    trimesh STEP extraction (36.4mm)
  J_body:   trimesh STEP extraction (0.0158 kg*m^2)
  Ku:       hardware oscillation test (0.12)
"""

import numpy as np

# ── Robot body (STEP extraction + scale confirmation) ──────────────────────
m_body   = 1.0000   # kg  scale-confirmed
l_com    = 0.0364   # m   CoM height above wheel axle (from STEP)
J_body   = 0.015747 # kg*m^2  pitch inertia (from STEP, Z-axis)

# ── Wheels ─────────────────────────────────────────────────────────────────
r_wheel  = 0.0325   # m  65mm diameter confirmed
m_wheel  = 0.040    # kg  estimate per wheel

# ── Motor (MG513P30 30:1 gearbox, 12V) ────────────────────────────────────
gear_ratio  = 30.0
V_supply    = 12.0
R_a         = 2.5    # Ohm (estimate)
K_e         = 0.030  # V*s/rad (estimate)
K_t         = 0.030  # N*m/A = K_e
deadband    = 0.10   # normalized (estimate)

# ── Control interface ──────────────────────────────────────────────────────
DIFF_MAX_RPM  = 150.0
setpoint_deg  = -5.0
fall_threshold = 35.0
control_hz    = 50.0
control_dt    = 0.020

# ── Constants ──────────────────────────────────────────────────────────────
g = 9.81

# ── Hardware observations ──────────────────────────────────────────────────
Ku = 0.12    # ultimate gain (oscillation at Kd=0.01)
Tu = 1.37    # estimated from omega_n = 4.57 rad/s
