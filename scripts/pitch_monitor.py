#!/usr/bin/env python3
"""
pitch_monitor.py — Real-time pitch monitor + auto-tune via ROS2 topics

Reads /auro/imu/data quaternion via SSH → VM agent, converts to pitch degrees.
Detects oscillation, measures Tu, applies Z-N for firmware-ready gains.

Usage:
    python3 scripts/pitch_monitor.py
    python3 scripts/pitch_monitor.py --host arif@10.10.9.249
    python3 scripts/pitch_monitor.py --log session.csv

Keyboard (while running):
    t  — compute Tu and print Z-N gains
    r  — reset peak buffer
    q  — quit
"""

import subprocess
import threading
import time
import sys
import re
import math
import argparse
import select
import os
from collections import deque
from datetime import datetime

# ── Constants ──────────────────────────────────────────────────────────────
VM_HOST   = 'arif@10.10.9.249'
TOPIC     = '/auro/imu/data'
SP_DEG    = -5.0
Ku        = 0.12
FALL_DEG  = 35.0

# ── Pitch buffer with peak detection ──────────────────────────────────────
class PitchTracker:
    def __init__(self):
        self.history = deque(maxlen=1500)  # 30s at 50Hz
        self.peaks = deque(maxlen=30)
        self.valleys = deque(maxlen=30)
        self.lock = threading.Lock()

    def add(self, t, pitch):
        with self.lock:
            self.history.append((t, pitch))
            n = len(self.history)
            if n >= 3:
                _, v0 = self.history[-3]
                _, v1 = self.history[-2]
                _, v2 = self.history[-1]
                t1 = self.history[-2][0]
                if v1 > v0 and v1 > v2 and v1 > SP_DEG + 1.5:
                    self.peaks.append((t1, v1))
                elif v1 < v0 and v1 < v2 and v1 < SP_DEG - 1.5:
                    self.valleys.append((t1, v1))

    def measure_Tu(self):
        """Period from consecutive same-sign peaks"""
        with self.lock:
            now = time.monotonic()
            for buf in [self.peaks, self.valleys]:
                recent = [(t, v) for t, v in buf if now - t < 20.0]
                if len(recent) >= 3:
                    periods = [recent[i+1][0] - recent[i][0]
                               for i in range(len(recent)-1)]
                    Tu = sum(periods) / len(periods)
                    if 0.1 < Tu < 3.0:
                        return Tu
            return None

    def amplitude(self):
        with self.lock:
            now = time.monotonic()
            vals = [v for t, v in self.history if now - t < 5.0]
            if len(vals) < 5:
                return 0.0
            return max(vals) - min(vals)

    def current(self):
        with self.lock:
            return self.history[-1][1] if self.history else 0.0

    def reset(self):
        with self.lock:
            self.peaks.clear()
            self.valleys.clear()


def quat_to_pitch_deg(y, w):
    """Convert quaternion (x=0, z=0) to pitch in degrees"""
    return 2.0 * math.atan2(y, w) * (180.0 / math.pi)


def zn_gains(Ku_val, Tu_s):
    """Z-N PID"""
    Kp = 0.6 * Ku_val
    Ki = 2.0 * Kp / Tu_s
    Kd = Kp * Tu_s / 8.0
    return Kp, Ki, Kd


def zn_pd(Ku_val, Tu_s):
    """Z-N PD only (safer for balancing)"""
    Kp = 0.8 * Ku_val
    Kd = Kp * Tu_s / 8.0
    return Kp, 0.0, Kd


def print_gains(Tu):
    print(f"\n{'='*60}")
    print(f"  Ku = {Ku}  (hardware confirmed)")
    print(f"  Tu = {Tu:.3f}s  (measured)")
    print(f"{'='*60}")

    methods = [
        ("Z-N PD only (safe)",      zn_pd(Ku, Tu)),
        ("Z-N classic PID",         zn_gains(Ku, Tu)),
    ]
    for name, (Kp, Ki, Kd) in methods:
        print(f"\n  {name}:")
        print(f"    Kp={Kp:.4f}  Ki={Ki:.6f}  Kd={Kd:.5f}")
        print(f"    #define BALANCE_ANGLE_KP  ({Kp:.4f}f)")
        print(f"    #define BALANCE_ANGLE_KI  ({Ki:.6f}f)")
        print(f"    #define BALANCE_ANGLE_KD  ({Kd:.5f}f)")

    print(f"\n  SIM-02 ladder (for comparison):")
    print(f"    [1] Kp=0.12 Kd=0.03  (current)")
    print(f"    [2] Kp=0.15 Kd=0.05")
    print(f"    [3] Kp=0.20 Kd=0.08  (sim best)")
    print(f"{'='*60}\n")


def reader_thread(host, tracker, stop_event, log_fh):
    """SSH to VM, subscribe to IMU topic, parse quaternion → pitch"""
    cmd = [
        'ssh', '-o', 'StrictHostKeyChecking=no', host,
        'source /opt/ros/humble/setup.bash && '
        'export FASTRTPS_DEFAULT_PROFILES_FILE=~/disable_fastdds_shm.xml && '
        f'ros2 topic echo --qos-reliability best_effort {TOPIC} '
        '--field orientation 2>/dev/null'
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True)

    y_val = None
    for line in proc.stdout:
        if stop_event.is_set():
            break
        line = line.strip()

        # Parse YAML-style output: "y: -0.032"
        m = re.match(r'y:\s+([-\d.eE]+)', line)
        if m:
            y_val = float(m.group(1))
            continue

        m = re.match(r'w:\s+([-\d.eE]+)', line)
        if m and y_val is not None:
            w_val = float(m.group(1))
            pitch = quat_to_pitch_deg(y_val, w_val)
            t = time.monotonic()
            tracker.add(t, pitch)

            if log_fh:
                log_fh.write(f"{t:.3f},{pitch:.3f}\n")
                log_fh.flush()

            y_val = None

    proc.terminate()


def main():
    ap = argparse.ArgumentParser(description='Pitch monitor + auto-tune')
    ap.add_argument('--host', default=VM_HOST)
    ap.add_argument('--log', default=None, metavar='FILE')
    args = ap.parse_args()

    tracker = PitchTracker()
    stop = threading.Event()
    log_fh = open(args.log, 'w') if args.log else None

    print(f"Connecting to {args.host}...")
    print(f"Subscribing to {TOPIC} (quaternion → pitch degrees)")
    print(f"Setpoint: {SP_DEG}°  Ku: {Ku}")
    print(f"Keys: [t]=tune  [r]=reset  [q]=quit\n")

    rt = threading.Thread(target=reader_thread,
                          args=(args.host, tracker, stop, log_fh),
                          daemon=True)
    rt.start()

    # Non-blocking keyboard check
    import termios, tty
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)

    try:
        tty.setcbreak(fd)
        last_print = 0

        while not stop.is_set():
            # Keyboard
            if select.select([sys.stdin], [], [], 0.0)[0]:
                ch = sys.stdin.read(1).lower()
                if ch == 'q':
                    break
                elif ch == 't':
                    Tu = tracker.measure_Tu()
                    if Tu:
                        print_gains(Tu)
                    else:
                        print("  Not enough peaks yet — wait for oscillation")
                elif ch == 'r':
                    tracker.reset()
                    print("  Peaks reset")

            # Display at ~5Hz
            now = time.monotonic()
            if now - last_print >= 0.2:
                last_print = now
                pitch = tracker.current()
                amp = tracker.amplitude()
                Tu = tracker.measure_Tu()
                err = pitch - SP_DEG

                # Compact one-line display
                bar_w = 40
                bar_pos = int((pitch + FALL_DEG) / (2 * FALL_DEG) * bar_w)
                bar_pos = max(0, min(bar_w - 1, bar_pos))
                sp_pos = int((SP_DEG + FALL_DEG) / (2 * FALL_DEG) * bar_w)
                bar = ['.'] * bar_w
                if 0 <= sp_pos < bar_w:
                    bar[sp_pos] = '|'
                bar[bar_pos] = '#'

                Tu_str = f"Tu={Tu:.3f}s" if Tu else "Tu=---"
                osc = "OSC" if amp > 5.0 and Tu else "   "

                line = (f"\r  pitch={pitch:+6.1f}° err={err:+5.1f}° "
                        f"amp={amp:4.1f}° {Tu_str} {osc}  "
                        f"[{''.join(bar)}]")
                print(line, end='', flush=True)

            time.sleep(0.05)

    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        stop.set()
        if log_fh:
            log_fh.close()

        # Final summary
        Tu = tracker.measure_Tu()
        print(f"\n\n{'='*60}")
        print("SESSION SUMMARY")
        print(f"{'='*60}")
        if Tu:
            print(f"  Tu measured: {Tu:.3f}s")
            print_gains(Tu)
        else:
            print("  No oscillation detected — Tu not measured")
            print("  To measure Tu: run with Kp=0.12, Ki=0, Kd=0")
            print("  Hold robot upright, enable balance, wait for oscillation")
        print()


if __name__ == '__main__':
    main()
