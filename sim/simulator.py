#!/usr/bin/env python3
"""
simulator.py — nonlinear inverted pendulum + 50Hz discrete PID

Euler integration at 1kHz (matching ISR rate).
PID operates in DEGREES (matching firmware Balance_Tick).
Motor modeled as first-order lag + deadband.
"""

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os, sys

sys.path.insert(0, os.path.dirname(__file__))
import params as P


def compute_K_plant():
    """Effective torque per unit g_diff_linear (N*m)"""
    tau_stall_per_motor = P.K_t * P.V_supply / P.R_a * P.gear_ratio
    tau_stall_total = 2 * tau_stall_per_motor
    efficiency = 0.35
    return tau_stall_total * efficiency

K_PLANT = compute_K_plant()


def simulate_balance(Kp, Ki, Kd,
                     initial_tilt_deg=5.0,
                     duration=5.0,
                     integral_max=0.3,
                     output_max=0.8,
                     deriv_filter=0.3,
                     verbose=False):
    m = P.m_body
    l = P.l_com
    J_eff = P.J_body + m * l**2
    mgl = m * P.g * l
    deadband = P.deadband

    dt_phys = 0.001       # 1kHz physics (matches ISR)
    dt_ctrl = P.control_dt  # 20ms = 50Hz
    tau_motor = 0.05      # motor lag (s)

    # PID state
    integral = 0.0
    prev_error = 0.0
    prev_deriv = 0.0
    u_cmd = 0.0           # PID output
    u_actual = 0.0        # after motor lag

    # Physics state
    theta = np.radians(initial_tilt_deg)
    theta_dot = 0.0

    FALL_RAD = np.radians(P.fall_threshold)

    # Storage
    N = int(duration / dt_phys) + 1
    t_arr = np.zeros(N)
    th_arr = np.zeros(N)
    u_arr = np.zeros(N)

    t = 0.0
    t_next_ctrl = 0.0
    cost = 0.0
    step = 0

    for step in range(N):
        t = step * dt_phys
        t_arr[step] = t
        th_arr[step] = np.degrees(theta)
        u_arr[step] = u_cmd

        # ── 50Hz PID update ────────────────────────────────────────────────
        if t >= t_next_ctrl:
            # Error in DEGREES (matching firmware: error = pitch - setpoint)
            # Positive theta = leaning forward → positive error → positive u
            # → forward motor → restoring torque (pushes base under CoM)
            error_deg = np.degrees(theta)

            deriv_raw = (error_deg - prev_error) / dt_ctrl
            deriv = deriv_filter * deriv_raw + (1 - deriv_filter) * prev_deriv

            integral = np.clip(integral + Ki * error_deg * dt_ctrl,
                               -integral_max, integral_max)

            u_cmd = np.clip(Kp * error_deg + integral + Kd * deriv,
                            -output_max, output_max)

            if verbose and step < 200:
                print(f"  t={t:.3f} θ={np.degrees(theta):+.2f}° "
                      f"err={error_deg:+.2f}° u={u_cmd:+.4f}")

            prev_error = error_deg
            prev_deriv = deriv
            t_next_ctrl += dt_ctrl

        # ── Motor lag (first-order) ────────────────────────────────────────
        alpha = dt_phys / tau_motor
        u_actual += alpha * (u_cmd - u_actual)

        # ── Motor torque with deadband ─────────────────────────────────────
        if abs(u_actual) < deadband:
            tau = 0.0
        else:
            u_eff = np.sign(u_actual) * (abs(u_actual) - deadband) / (1 - deadband)
            tau = u_eff * K_PLANT

        # ── Physics (Euler integration) ────────────────────────────────────
        theta_ddot = (mgl * np.sin(theta) - tau) / J_eff
        theta_ddot -= 0.05 * theta_dot  # viscous damping

        theta_dot += theta_ddot * dt_phys
        theta += theta_dot * dt_phys

        cost += t * abs(theta) * dt_phys

        # ── Fall detection ─────────────────────────────────────────────────
        if abs(theta) > FALL_RAD:
            if verbose:
                print(f"  FELL at t={t:.3f}s θ={np.degrees(theta):.1f}°")
            cost += (duration - t) * FALL_RAD * 10
            t_arr = t_arr[:step+1]
            th_arr = th_arr[:step+1]
            u_arr = u_arr[:step+1]
            return t_arr, th_arr, u_arr, cost, False

    return t_arr, th_arr, u_arr, cost, True


def plot_response(t, theta, u, gains, title="", filename=None):
    os.makedirs('sim/plots', exist_ok=True)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)

    ax1.plot(t, theta, 'b-', lw=1.5)
    ax1.axhline(0, color='k', ls='--', alpha=0.4)
    ax1.axhline(35, color='r', ls=':', alpha=0.4, label='fall')
    ax1.axhline(-35, color='r', ls=':', alpha=0.4)
    ax1.set_ylabel('Pitch (deg)')
    ax1.set_title(f'{title}  Kp={gains[0]:.4f} Ki={gains[1]:.5f} Kd={gains[2]:.5f}')
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    ax2.plot(t, u, 'g-', lw=1.5)
    ax2.axhline(0.8, color='r', ls=':', alpha=0.4)
    ax2.axhline(-0.8, color='r', ls=':', alpha=0.4)
    ax2.set_ylabel('Output (norm)')
    ax2.set_xlabel('Time (s)')
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    if filename is None:
        filename = f"sim/plots/resp_Kp{gains[0]:.4f}_Kd{gains[2]:.5f}.png"
    plt.savefig(filename, dpi=150)
    plt.close()
    return filename


if __name__ == '__main__':
    print(f"K_PLANT  = {K_PLANT:.3f} N*m per unit output")
    print(f"mgl      = {P.m_body * P.g * P.l_com:.3f} N*m at 1 rad")
    print(f"J_eff    = {P.J_body + P.m_body * P.l_com**2:.5f} kg*m^2")
    print(f"deadband = {P.deadband}")
    print(f"Ratio    = {K_PLANT / (P.m_body * P.g * P.l_com):.2f}x\n")

    # Debug trace for Kp=0.12
    print("=== Debug trace: Kp=0.12 Kd=0.01 tilt=5deg ===")
    simulate_balance(0.12, 0.0, 0.01, initial_tilt_deg=5.0,
                     duration=0.5, verbose=True)

    print(f"\n{'Label':<35s} {'Cost':>8s} {'Stable':>7s} {'Max|θ|':>7s}")
    print("-" * 62)

    tests = [
        (0.12,  0.0,    0.01,   "Kp=0.12 Kd=0.01 (current fw)"),
        (0.12,  0.0,    0.03,   "Kp=0.12 Kd=0.03"),
        (0.15,  0.0,    0.03,   "Kp=0.15 Kd=0.03"),
        (0.15,  0.0,    0.05,   "Kp=0.15 Kd=0.05"),
        (0.20,  0.0,    0.05,   "Kp=0.20 Kd=0.05"),
        (0.20,  0.0,    0.08,   "Kp=0.20 Kd=0.08"),
        (0.25,  0.0,    0.08,   "Kp=0.25 Kd=0.08"),
        (0.25,  0.0,    0.10,   "Kp=0.25 Kd=0.10"),
        (0.30,  0.0,    0.10,   "Kp=0.30 Kd=0.10"),
        (0.40,  0.0,    0.10,   "Kp=0.40 Kd=0.10"),
        (0.40,  0.0,    0.15,   "Kp=0.40 Kd=0.15"),
        (0.50,  0.0,    0.15,   "Kp=0.50 Kd=0.15"),
    ]

    best_cost = 1e9
    best = None

    for Kp, Ki, Kd, label in tests:
        t, th, u, cost, stable = simulate_balance(
            Kp, Ki, Kd, initial_tilt_deg=5.0, duration=5.0)
        max_th = np.max(np.abs(th))
        status = "YES" if stable else "FELL"
        print(f"{label:<35s} {cost:8.3f} {status:>7s} {max_th:6.1f}°")
        plot_response(t, th, u, [Kp, Ki, Kd], title=label)

        if stable and cost < best_cost:
            best_cost = cost
            best = (Kp, Ki, Kd, label)

    if best:
        print(f"\nBest: {best[3]}  cost={best_cost:.3f}")
        for tilt in [3, 5, 8, 10]:
            t, th, u, cost, ok = simulate_balance(
                best[0], best[1], best[2], initial_tilt_deg=tilt, duration=5.0)
            print(f"  tilt={tilt:+d}°: {'OK' if ok else 'FELL'} cost={cost:.3f}")
            plot_response(t, th, u, list(best[:3]),
                          title=f"Best @ {tilt}deg",
                          filename=f"sim/plots/best_tilt{tilt}.png")
    else:
        print("\nNo stable config — plant model needs calibration")
    print(f"\nPlots: sim/plots/")
