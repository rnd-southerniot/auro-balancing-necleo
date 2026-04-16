"""
params.py — physical parameters for auro-balancing-necleo simulation

Sources:
  Mass:     hardware scale (1000g confirmed)
  Ku:       hardware oscillation test (Kp=0.12 → sustained oscillation)
  Setpoint: hardware pitch measurement (-5.0 deg)
  CoM/Inertia: estimated — UPDATE from Fusion 360 or physical measurement

MEASURE AND UPDATE:
  r_wheel — measure physical wheel diameter with calipers
  l_com   — Fusion 360 Inspect → Center of Mass, or balance test
  J_body  — Fusion 360 Inspect → Moments of Inertia
"""

# ── Robot body ─────────────────────────────────────────────────────────────────
m_body  = 1.000    # kg  total mass (scale confirmed)
l_com   = 0.085    # m   CoM height above axle — ESTIMATE, update from CAD
J_body  = 0.008    # kg*m^2  pitch inertia — ESTIMATE, update from CAD

# ── Wheels ─────────────────────────────────────────────────────────────────────
r_wheel = 0.0325   # m   wheel radius (65mm diameter from config.h)
m_wheel = 0.040    # kg  one wheel estimated

# ── Motor (MG513P30, 30:1 gearbox, 12V) ───────────────────────────────────────
gear_ratio  = 30.0
V_supply    = 12.0   # V
R_a         = 3.0    # Ohm  armature resistance (estimate — measure with multimeter)
K_e         = 0.025  # V*s/rad  back-EMF constant (estimate)
K_t         = 0.025  # N*m/A  torque constant (= K_e in SI)
deadband    = 0.10   # normalized — minimum output to move motor

# ── Control interface (must match firmware) ────────────────────────────────────
DIFF_MAX_RPM    = 150.0   # g_diff_linear * this = motor RPM setpoint
setpoint_deg    = -5.0    # degrees (hardware-confirmed)
fall_threshold  = 35.0    # degrees
control_hz      = 50.0    # Hz
control_dt      = 1.0 / control_hz

# ── Hardware observation ───────────────────────────────────────────────────────
Ku = 0.12   # ultimate gain (sustained oscillation observed on hardware)
Tu = 0.4    # oscillation period (seconds) — ESTIMATE, measure on hardware

# ── Physical constants ─────────────────────────────────────────────────────────
g = 9.81
