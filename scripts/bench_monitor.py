#!/usr/bin/env python3
"""
Bench test monitor for auro-balancing-necleo.

Decodes binary telemetry frames from STM32 firmware and sends commands.
Protocol: [0xAA][0x01][MSG_TYPE][PAYLOAD...][CRC16_LO][CRC16_HI]
CRC16-CCITT: init=0xFFFF, poly=0x1021, no reflect, no final XOR.

Usage:
    # Monitor telemetry (auto-detect port)
    python3 scripts/bench_monitor.py

    # Specify port
    python3 scripts/bench_monitor.py --port /dev/tty.usbmodem1301

    # Send Motor A RPM command
    python3 scripts/bench_monitor.py --cmd rpm_a 30

    # Send Motor B RPM command
    python3 scripts/bench_monitor.py --cmd rpm_b -30

    # Send E-stop
    python3 scripts/bench_monitor.py --cmd estop

    # Set Motor A mode to RPM
    python3 scripts/bench_monitor.py --cmd mode_a rpm

    # Set Motor A mode to idle
    python3 scripts/bench_monitor.py --cmd mode_a idle

    # Send keepalive
    python3 scripts/bench_monitor.py --cmd keepalive
"""

import argparse
import glob
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip3 install pyserial")
    sys.exit(1)

# ── Protocol constants ──────────────────────────────────────

SYNC = 0xAA
VERSION = 0x01
BAUD = 921600

# Message types
MSG_TELEM_FAST = 0x10
MSG_TELEM_POSE = 0x12
MSG_CMD_RPM = 0x20
MSG_CMD_POSITION = 0x21
MSG_CMD_MODE = 0x23
MSG_CMD_ESTOP = 0x24
MSG_CMD_KEEPALIVE = 0x29
MSG_CMD_RPM_B = 0x40
MSG_CMD_MODE_B = 0x43
MSG_RESP_ACK = 0x30
MSG_RESP_NACK = 0x31

# Payload sizes
PAYLOAD_SIZES = {
    MSG_TELEM_FAST: 44,
    MSG_TELEM_POSE: 24,
    MSG_CMD_RPM: 4,
    MSG_CMD_POSITION: 4,
    MSG_CMD_MODE: 1,
    MSG_CMD_ESTOP: 0,
    MSG_CMD_KEEPALIVE: 0,
    MSG_CMD_RPM_B: 4,
    MSG_CMD_MODE_B: 1,
    MSG_RESP_ACK: 1,
    MSG_RESP_NACK: 2,
}

# Control modes
CTRL_IDLE = 0x00
CTRL_RPM = 0x01
CTRL_POSITION = 0x02

MODE_MAP = {"idle": CTRL_IDLE, "rpm": CTRL_RPM, "position": CTRL_POSITION}


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc


def encode_frame(msg_type: int, payload: bytes = b"") -> bytes:
    header = bytes([SYNC, VERSION, msg_type])
    crc_data = bytes([VERSION, msg_type]) + payload
    crc = crc16_ccitt(crc_data)
    return header + payload + struct.pack("<H", crc)


def decode_telem_fast(payload: bytes) -> dict:
    if len(payload) != 44:
        return {"error": f"bad length {len(payload)}"}
    fields = struct.unpack("<IffffffffffffffBBBB", payload)
    return {
        "ts_ms": fields[0],
        "m1_rpm": fields[1],
        "m1_pos": fields[2],
        "m1_cur_mA": fields[3],
        "m1_pid": fields[4],
        "m2_rpm": fields[5],
        "m2_pos": fields[6],
        "m2_cur_mA": fields[7],
        "m2_pid": fields[8],
        "batt_V": fields[9],
        "m1_fault": fields[10],
        "m2_fault": fields[11],
        "m1_mode": fields[12],
        "m2_mode": fields[13],
    }


def decode_telem_pose(payload: bytes) -> dict:
    if len(payload) != 24:
        return {"error": f"bad length {len(payload)}"}
    fields = struct.unpack("<Ifffff", payload)
    return {
        "ts_ms": fields[0],
        "x_m": fields[1],
        "y_m": fields[2],
        "theta_deg": fields[3],
        "v_m_s": fields[4],
        "w_deg_s": fields[5],
    }


def detect_port() -> str:
    candidates = glob.glob("/dev/tty.usbmodem*")
    if not candidates:
        candidates = glob.glob("/dev/ttyACM*")
    if not candidates:
        print("ERROR: No USB serial device found.")
        print("       Check USB cable and ST-LINK enumeration.")
        sys.exit(1)
    port = candidates[0]
    print(f"Auto-detected port: {port}")
    return port


def send_command(port: str, cmd: str, args: list):
    ser = serial.Serial(port, BAUD, timeout=1)
    time.sleep(0.1)

    if cmd == "rpm_a":
        rpm = float(args[0])
        # First set mode to RPM
        frame = encode_frame(MSG_CMD_MODE, struct.pack("<B", CTRL_RPM))
        ser.write(frame)
        time.sleep(0.05)
        # Then set RPM setpoint
        frame = encode_frame(MSG_CMD_RPM, struct.pack("<f", rpm))
        ser.write(frame)
        print(f"Sent: Motor A RPM = {rpm}")

    elif cmd == "rpm_b":
        rpm = float(args[0])
        frame = encode_frame(MSG_CMD_MODE_B, struct.pack("<B", CTRL_RPM))
        ser.write(frame)
        time.sleep(0.05)
        frame = encode_frame(MSG_CMD_RPM_B, struct.pack("<f", rpm))
        ser.write(frame)
        print(f"Sent: Motor B RPM = {rpm}")

    elif cmd == "mode_a":
        mode = MODE_MAP.get(args[0], CTRL_IDLE)
        frame = encode_frame(MSG_CMD_MODE, struct.pack("<B", mode))
        ser.write(frame)
        print(f"Sent: Motor A mode = {args[0]} ({mode:#x})")

    elif cmd == "mode_b":
        mode = MODE_MAP.get(args[0], CTRL_IDLE)
        frame = encode_frame(MSG_CMD_MODE_B, struct.pack("<B", mode))
        ser.write(frame)
        print(f"Sent: Motor B mode = {args[0]} ({mode:#x})")

    elif cmd == "estop":
        frame = encode_frame(MSG_CMD_ESTOP)
        ser.write(frame)
        print("Sent: E-STOP")

    elif cmd == "keepalive":
        frame = encode_frame(MSG_CMD_KEEPALIVE)
        ser.write(frame)
        print("Sent: Keepalive")

    else:
        print(f"Unknown command: {cmd}")
        ser.close()
        return

    # Wait for ACK/NACK
    time.sleep(0.1)
    if ser.in_waiting:
        resp = ser.read(ser.in_waiting)
        if SYNC in resp:
            idx = resp.index(SYNC)
            if idx + 4 < len(resp):
                msg_type = resp[idx + 2]
                if msg_type == MSG_RESP_ACK:
                    print("  -> ACK received")
                elif msg_type == MSG_RESP_NACK:
                    print("  -> NACK received")
                else:
                    print(f"  -> Response type: {msg_type:#x}")
    ser.close()


def monitor(port: str):
    ser = serial.Serial(port, BAUD, timeout=0.5)
    print(f"Monitoring {port} @ {BAUD} baud. Ctrl+C to stop.\n")
    print(f"{'Time':>8s}  {'M1 RPM':>8s}  {'M2 RPM':>8s}  {'Batt V':>7s}  "
          f"{'CT_A mA':>8s}  {'CT_B mA':>8s}  {'M1mode':>6s}  {'M2mode':>6s}  "
          f"{'Faults':>6s}")
    print("-" * 90)

    mode_names = {0: "IDLE", 1: "RPM", 2: "POS", 3: "TUNE", 4: "DIFF"}
    frame_count = 0
    crc_errors = 0
    buf = bytearray()

    try:
        while True:
            chunk = ser.read(256)
            if not chunk:
                continue
            buf.extend(chunk)

            while len(buf) >= 5:  # minimum frame size
                # Find sync
                try:
                    idx = buf.index(SYNC)
                except ValueError:
                    buf.clear()
                    break

                if idx > 0:
                    buf = buf[idx:]

                if len(buf) < 3:
                    break

                version = buf[1]
                msg_type = buf[2]

                plen = PAYLOAD_SIZES.get(msg_type, None)
                if plen is None or version != VERSION:
                    buf = buf[1:]
                    continue

                frame_len = 3 + plen + 2  # sync+ver+type + payload + crc
                if len(buf) < frame_len:
                    break

                payload = bytes(buf[3 : 3 + plen])
                crc_rx = struct.unpack("<H", buf[3 + plen : 3 + plen + 2])[0]
                crc_data = bytes(buf[1 : 3 + plen])
                crc_calc = crc16_ccitt(crc_data)

                buf = buf[frame_len:]

                if crc_rx != crc_calc:
                    crc_errors += 1
                    continue

                frame_count += 1

                if msg_type == MSG_TELEM_FAST:
                    t = decode_telem_fast(payload)
                    m1m = mode_names.get(t["m1_mode"], "?")
                    m2m = mode_names.get(t["m2_mode"], "?")
                    faults = t["m1_fault"] | t["m2_fault"]
                    print(
                        f"{t['ts_ms']/1000:8.2f}  "
                        f"{t['m1_rpm']:8.1f}  {t['m2_rpm']:8.1f}  "
                        f"{t['batt_V']:7.2f}  "
                        f"{t['m1_cur_mA']:8.1f}  {t['m2_cur_mA']:8.1f}  "
                        f"{m1m:>6s}  {m2m:>6s}  "
                        f"{'OK' if faults == 0 else f'0x{faults:02X}':>6s}"
                    )

                elif msg_type == MSG_TELEM_POSE:
                    p = decode_telem_pose(payload)
                    print(
                        f"  POSE  x={p['x_m']:.3f}m  y={p['y_m']:.3f}m  "
                        f"th={p['theta_deg']:.1f}deg  "
                        f"v={p['v_m_s']:.3f}m/s  w={p['w_deg_s']:.1f}deg/s"
                    )

    except KeyboardInterrupt:
        print(f"\n\nFrames: {frame_count}  CRC errors: {crc_errors}")
        ser.close()


def main():
    parser = argparse.ArgumentParser(description="Bench test monitor for auro-balancing-necleo")
    parser.add_argument("--port", help="Serial port (auto-detect if omitted)")
    parser.add_argument("--cmd", nargs="+", help="Send command: rpm_a <val> | rpm_b <val> | mode_a idle|rpm | estop | keepalive")
    args = parser.parse_args()

    port = args.port or detect_port()

    if args.cmd:
        send_command(port, args.cmd[0], args.cmd[1:])
    else:
        monitor(port)


if __name__ == "__main__":
    main()
