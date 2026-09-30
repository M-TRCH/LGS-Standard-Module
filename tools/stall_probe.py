"""Reproduce -- and then disprove -- the libmodbus byte-timeout stall.

    python tools/stall_probe.py --gw 192.168.0.202 --id 21            # run it
    python tools/stall_probe.py --gw 192.168.0.202 --id 21 --dry      # show frames only

BENCH USE ONLY. This deliberately sends one malformed request to one module and
then keeps the bus busy for a few seconds. On firmware before v3.5.1 the module
sits inside libmodbus for the whole time and the 4 s watchdog reboots it; on
v3.5.1 it shrugs the frame off within a frame gap and keeps answering. Point it
at a module on the bench gateway, never at a cabinet in service.

Why this frame: `_modbus_receive_msg` trusts the byte-count field of an FC16
(write multiple registers) to decide how many more bytes to wait for. A count
of 240 with only two data bytes present is exactly the shape a flipped bit or
a frame cut by the RS485 switch hub produces. The frame is CRC-valid (the
gateway appends the CRC), so it reaches the parser; its quantity field is 0,
so even the one-in-65536 case where the accumulated garbage passes CRC ends in
an ILLEGAL DATA VALUE exception and writes nothing. The follow-up "traffic" is
8-byte reads addressed to a unit that does not exist (200): every module hears
them, none answers, and the gateway's 300 ms slave timeout paces them.

Goes through the gateway on purpose: no USB-RS485 adapter needed, and it is
the same path the field traffic takes. Standard library only.
"""
import argparse
import socket
import struct
import sys
import time

MBAP_LEN = 7
QUEEN = "192.168.0.227"


class Gw:
    def __init__(self, host: str, port: int = 502, timeout: float = 1.5) -> None:
        self.sock = socket.create_connection((host, port), timeout=5.0)
        self.sock.settimeout(timeout)
        self.tid = 0

    def close(self) -> None:
        self.sock.close()

    def _send(self, unit: int, pdu: bytes) -> int:
        self.tid = (self.tid + 1) & 0xFFFF
        frame = struct.pack(">HHHB", self.tid, 0, len(pdu) + 1, unit) + pdu
        self.sock.sendall(frame)
        return self.tid

    def _recv(self):
        try:
            head = self.sock.recv(MBAP_LEN)
        except socket.timeout:
            return None
        if len(head) < MBAP_LEN:
            return None
        _tid, _proto, length, _unit = struct.unpack(">HHHB", head)
        body = b""
        while len(body) < length - 1:
            try:
                chunk = self.sock.recv(length - 1 - len(body))
            except socket.timeout:
                return None
            if not chunk:
                return None
            body += chunk
        return body

    def read_regs(self, unit: int, addr: int, count: int):
        """Holding registers as a list, or None when nothing came back."""
        self._send(unit, struct.pack(">BHH", 0x03, addr, count))
        body = self._recv()
        if not body or body[0] != 0x03 or len(body) < 2 + 2 * count:
            return None
        return list(struct.unpack(">%dH" % count, body[2:2 + 2 * count]))

    def send_raw(self, unit: int, pdu: bytes) -> None:
        """Fire a PDU and swallow whatever (if anything) comes back."""
        self._send(unit, pdu)
        self._recv()


def snapshot(gw: Gw, unit: int):
    ident = gw.read_regs(unit, 5, 4)        # uptime hi/lo, boots, reset cause
    iwdg = gw.read_regs(unit, 410, 1)
    if ident is None or iwdg is None:
        return None
    return {"uptime_s": (ident[0] << 16) | ident[1], "boots": ident[2],
            "cause": ident[3], "iwdg": iwdg[0]}


def cause_text(bits: int) -> str:
    names = ["IWDG", "SW", "POWER", "PIN", "WWDG", "LPWR", "OBL"]
    return " ".join(n for i, n in enumerate(names) if bits & (1 << i)) or f"raw {bits}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--gw", required=True, help="gateway IP (a BENCH gateway)")
    ap.add_argument("--id", type=int, required=True, help="module unit id under test")
    ap.add_argument("--seconds", type=float, default=6.0,
                    help="how long to keep the bus busy after the bad frame")
    ap.add_argument("--dry", action="store_true", help="print the frames, send nothing")
    ap.add_argument("--force", action="store_true",
                    help="allow a gateway that is not obviously a bench")
    ap.add_argument("--feed-unit", type=int, default=200,
                    help="absent unit id the feed frames are addressed to. Behind a "
                         "switch hub pick an unused id ON THE TARGET'S ROW (e.g. 109 "
                         "for row 10) so the hub keeps that channel selected")
    args = ap.parse_args()

    if args.gw == QUEEN and not args.force:
        sys.exit("refusing: that is the pilot cabinet, not a bench (--force to override)")

    # FC16, start 400, quantity 0, byte count 240, two data bytes. CRC is
    # added by the gateway, so the module receives a well-formed header that
    # promises 240 bytes which never come.
    bad_pdu = struct.pack(">BHHB", 0x10, 400, 0, 240) + b"\x00\x00"
    feed_pdu = struct.pack(">BHH", 0x03, 0, 1)         # 8 bytes on the wire

    print(f"bad frame  -> unit {args.id}: {bad_pdu.hex(' ')}")
    print(f"feed frame -> unit {args.feed_unit} x ~{args.seconds / 0.3:.0f}: {feed_pdu.hex(' ')}")
    if args.dry:
        return 0

    gw = Gw(args.gw)
    try:
        before = snapshot(gw, args.id)
        if before is None:
            sys.exit(f"unit {args.id} does not answer on {args.gw} — nothing sent")
        print(f"{time.strftime('%H:%M:%S')}  before: boots {before['boots']} "
              f"iwdg {before['iwdg']} uptime {before['uptime_s']} s "
              f"cause {cause_text(before['cause'])}")

        t0 = time.monotonic()
        gw.send_raw(args.id, bad_pdu)
        print(f"{time.strftime('%H:%M:%S')}  bad frame sent; feeding the bus "
              f"for {args.seconds:.0f} s")
        answered_mid = None
        n = 0
        while time.monotonic() - t0 < args.seconds:
            gw.send_raw(args.feed_unit, feed_pdu)
            n += 1
            if answered_mid is None and time.monotonic() - t0 >= 2.0:
                # Halfway through: is the module still with us?
                answered_mid = gw.read_regs(args.id, 7, 1) is not None
                print(f"{time.strftime('%H:%M:%S')}  mid-feed probe: "
                      f"{'ANSWERED' if answered_mid else 'silent'}")
        print(f"{time.strftime('%H:%M:%S')}  {n} feed frames sent")

        time.sleep(1.5)                     # let a stuck module time out or reboot
        after = snapshot(gw, args.id)
        if after is None:
            print("after: unit silent — wait and re-run snapshot by hand")
            return 2
        print(f"{time.strftime('%H:%M:%S')}  after:  boots {after['boots']} "
              f"iwdg {after['iwdg']} uptime {after['uptime_s']} s "
              f"cause {cause_text(after['cause'])}")

        rebooted = after["boots"] != before["boots"] or after["uptime_s"] < before["uptime_s"]
        if rebooted and (after["cause"] & 1):
            print("\nVERDICT: STALLED INTO THE WATCHDOG — the hole is open on this firmware")
            return 1
        if rebooted:
            print("\nVERDICT: rebooted, but not by IWDG — inspect the cause bits above")
            return 1
        print("\nVERDICT: no reset" + (" and it kept answering mid-feed" if answered_mid else "")
              + " — the receive loop is bounded")
        return 0
    finally:
        gw.close()


if __name__ == "__main__":
    raise SystemExit(main())
