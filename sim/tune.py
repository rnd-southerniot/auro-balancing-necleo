#!/usr/bin/env python3
"""
tune.py — differential_evolution PID gain optimizer

Sweeps Kp/Ki/Kd to minimize ITAE cost across multiple initial tilts.
Uses measured physical parameters from params.py.

Usage: python3 sim/tune.py
"""

import numpy as np
from scipy.optimize import differential_evolution
import time, os, sys

sys.path.insert(0, os.path.dirname(__file__))
from simulator import simulate_balance, plot_response
import params as P


def cost_function(gains, n_tilts=4):
    """Multi-scenario ITAE cost. Lower = better."""
    Kp, Ki, Kd = gains
    if Kp <= 0 or Kd < 0 or Ki < 0:
        return 1e9

    total = 0.0
    for tilt in [3.0, 5.0, 8.0, -5.0][:n_tilts]:
        _, _, _, cost, stable = simulate_balance(
            Kp, Ki, Kd, initial_tilt_deg=tilt, duration=5.0)
        total += cost
        if not stable:
            total += 100
    return total


def main():
    print("=" * 60)
    print("PID GAIN OPTIMIZER — auro-balancing-necleo")
    print("=" * 60)
    print(f"Hardware: Ku={P.Ku}, Tu={P.Tu}s")
    print(f"Mass={P.m_body}kg, l_com={P.l_com}m, r_wheel={P.r_wheel}m")

    # Z-N seed
    Kp_zn = 0.6 * P.Ku
    Kd_zn = Kp_zn * P.Tu / 8

    # Search bounds (around Z-N ± generous range)
    bounds_pd = [
        (0.02, 0.30),    # Kp
        (0.001, 0.10),   # Kd
    ]

    # Phase 1: PD only
    print(f"\nPhase 1: Optimize Kp + Kd (Ki=0)...")
    t0 = time.time()

    def cost_pd(x):
        return cost_function([x[0], 0.0, x[1]])

    res_pd = differential_evolution(
        cost_pd, bounds_pd,
        maxiter=80, popsize=15, tol=1e-4, seed=42, workers=1)
    Kp_opt, Kd_opt = res_pd.x
    print(f"  Done in {time.time()-t0:.1f}s")
    print(f"  PD: Kp={Kp_opt:.4f}  Kd={Kd_opt:.5f}  cost={res_pd.fun:.3f}")

    # Phase 2: Add Ki
    print(f"\nPhase 2: Add Ki...")

    def cost_ki(x):
        return cost_function([Kp_opt, x[0], Kd_opt])

    res_ki = differential_evolution(
        cost_ki, [(0.0, 0.5)],
        maxiter=40, popsize=10, seed=42, workers=1)
    Ki_opt = res_ki.x[0]
    print(f"  Ki={Ki_opt:.6f}  cost={res_ki.fun:.3f}")

    # Validate
    print(f"\nValidation:")
    all_ok = True
    for tilt in [3, 5, 8, 10, -5, -8]:
        t, th, u, cost, ok = simulate_balance(
            Kp_opt, Ki_opt, Kd_opt, initial_tilt_deg=tilt, duration=5.0)
        print(f"  tilt={tilt:+3d}°  cost={cost:.3f}  {'OK' if ok else 'FELL'}")
        if not ok:
            all_ok = False

    # Plot best
    t, th, u, cost, ok = simulate_balance(
        Kp_opt, Ki_opt, Kd_opt, initial_tilt_deg=5.0, duration=5.0)
    plot_response(t, th, u, [Kp_opt, Ki_opt, Kd_opt],
                  title="Optimized PID", filename="sim/plots/optimized.png")

    # Scale for hardware (70% of simulated)
    S = 0.70
    print(f"\n{'=' * 60}")
    print("OPTIMAL GAINS (simulation)")
    print(f"{'=' * 60}")
    print(f"  Kp = {Kp_opt:.4f}")
    print(f"  Ki = {Ki_opt:.6f}")
    print(f"  Kd = {Kd_opt:.5f}")
    print(f"  All stable: {all_ok}")

    print(f"\n{'=' * 60}")
    print("FIRMWARE VALUES (70% scale for hardware)")
    print(f"{'=' * 60}")
    print(f"  #define BALANCE_ANGLE_KP  ({Kp_opt*S:.4f}f)")
    print(f"  #define BALANCE_ANGLE_KI  ({Ki_opt*S:.6f}f)")
    print(f"  #define BALANCE_ANGLE_KD  ({Kd_opt*S:.5f}f)")


if __name__ == '__main__':
    main()
