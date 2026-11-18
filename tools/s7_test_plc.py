"""A test S7 PLC for HMI Studio, on this computer.

Runs an S7 server (python-snap7) with DB1 and the markers, and changes a few
values, so the runtime can read and write a "real" S7 PLC without hardware:

    pip install python-snap7
    python tools/s7_test_plc.py            # port 1102
    python tools/s7_test_plc.py --port 102 # the S7 port (needs admin rights)

In HMI Studio, add a PLC (Connections): type Siemens S7-1200 / S7-1500,
IP address 127.0.0.1:1102. Then tags on it:

    Level     Real  DB1.DBD0    a ramp 0..100, read only here
    Running   Bool  DB1.DBX4.0  toggles every 5 s
    Setpoint  Int   DB1.DBW6    written by the HMI; printed when it changes
    Start     Bool  M0.0        written by the HMI; printed when it changes

Stop with Ctrl+C.
"""

import argparse
import ctypes
import struct
import time


def load_server():
    """The Server class and the DB/marker area codes, for python-snap7 1.x to 3.x."""
    try:  # python-snap7 3.x
        from s7.server import Server
        from s7.type import SrvArea

        return Server, SrvArea.DB, SrvArea.MK
    except ImportError:
        pass
    from snap7.server import Server

    try:  # 2.x
        from snap7.type import SrvArea

        return Server, SrvArea.DB, SrvArea.MK
    except ImportError:  # 1.x
        from snap7.types import srvAreaDB, srvAreaMK

        return Server, srvAreaDB, srvAreaMK


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=1102, help="TCP port (default 1102)")
    args = parser.parse_args()

    server_class, area_db, area_mk = load_server()
    db1 = bytearray(64)
    markers = bytearray(64)
    server = server_class()
    # The server serves these buffers in place: changing them changes the PLC.
    server.register_area(area_db, 1, (ctypes.c_char * len(db1)).from_buffer(db1))
    server.register_area(area_mk, 0, (ctypes.c_char * len(markers)).from_buffer(markers))
    server.start(tcp_port=args.port)
    print(f"Test S7 PLC on port {args.port}: DB1 (64 bytes) and markers. Ctrl+C stops.")

    started = time.monotonic()
    last_setpoint = None
    last_start = None
    try:
        while True:
            seconds = time.monotonic() - started
            struct.pack_into(">f", db1, 0, (seconds * 5.0) % 100.0)  # DB1.DBD0
            running = int(seconds // 5) % 2 == 1
            db1[4] = (db1[4] | 1) if running else (db1[4] & ~1)  # DB1.DBX4.0
            setpoint = struct.unpack_from(">h", db1, 6)[0]  # DB1.DBW6
            start = bool(markers[0] & 1)  # M0.0
            if setpoint != last_setpoint:
                print(f"DB1.DBW6 (Setpoint) = {setpoint}")
                last_setpoint = setpoint
            if start != last_start:
                print(f"M0.0 (Start) = {start}")
                last_start = start
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()
        if hasattr(server, "destroy"):
            server.destroy()


if __name__ == "__main__":
    main()
