#!/usr/bin/env python3
"""PTY integration: select first of two files via Ctrl+T, type, save, quit."""
import errno
import os
import pty
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time


def drive(binary):
    with tempfile.TemporaryDirectory(prefix="likeedit-pty-") as folder:
        a, b = (os.path.join(folder, name) for name in ("one.c", "two.py"))
        with open(a, "w", encoding="utf-8") as f:
            f.write("int main;\n")
        with open(b, "w", encoding="utf-8") as f:
            f.write("print(1)\n")
        master, slave = pty.openpty()
        import fcntl
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
        env = os.environ.copy()
        env["TERM"] = "xterm-256color"
        env["LC_ALL"] = env.get("LC_ALL", "en_US.UTF-8" if sys.platform == "darwin" or "bsd" in sys.platform else "C.UTF-8")
        p = subprocess.Popen([binary, a, b], stdin=slave, stdout=slave,
                             stderr=slave, env=env, start_new_session=True)
        os.close(slave)
        deadline = time.monotonic() + 18
        output = bytearray()

        def send(keys, delay=0.35):
            os.write(master, keys)
            end = time.monotonic() + delay
            while time.monotonic() < end:
                r, _, _ = select.select([master], [], [], .06)
                if r:
                    try:
                        output.extend(os.read(master, 32768))
                    except OSError as ex:
                        if ex.errno != errno.EIO:
                            raise

        try:
            send(b"\x14", .7)   # Ctrl+T from COMMAND, picker opens
            send(b"k", .5)      # choose first buffer (terminal independent)
            send(b"\r", .5)      # enter EDIT mode
            send(b"X", .4)       # first character should be inserted
            send(b"\x13", .7)   # Ctrl+S save first buffer
            send(b"\x1b", 1.2)  # Esc back to COMMAND
            send(b":q\r", .9)  # graceful exit
            while p.poll() is None and time.monotonic() < deadline:
                send(b"", .1)
            if p.poll() is None:
                raise AssertionError(f"LikeEdit did not quit after :q; file={open(a).read()!r}; tail={output[-1500:]!r}")
            if p.returncode != 0:
                raise AssertionError(f"LikeEdit returned {p.returncode}; tail: {output[-500:]!r}")
            with open(a, "r", encoding="utf-8") as f:
                actual = f.read()
            if actual != "Xint main;\n":
                raise AssertionError(f"file selector or save failed: {actual!r}")
            with open(b, "r", encoding="utf-8") as f:
                assert f.read() == "print(1)\n"
            print("LikeEdit PTY selector/save/quit smoke passed")
        finally:
            if p.poll() is None:
                p.kill()
                p.wait(timeout=5)
            os.close(master)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: pty_smoke.py /path/to/LikeEdit")
    drive(os.path.abspath(sys.argv[1]))
