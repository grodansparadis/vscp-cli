#!/usr/bin/env python3
"""Tests for the VSCP command line tools.

Run by ctest (see CMakeLists.txt) or by hand:

  tests/test_tools.py --bindir build

Checks help/version, the interface configuration each option gives
(--print-config), command line errors and exit codes. On Linux it also sends
events over UDP on the loopback broadcast address (127.255.255.255) and
checks that vscp-dump receives them, plain and AES-128 + HMAC encrypted.
"""

import argparse
import json
import os
import platform
import subprocess
import sys
import time
import unittest

BINDIR = "."
EXE = ".exe" if os.name == "nt" else ""
TOOLS = ["vscp-send", "vscp-dump", "vscp-gen", "vscp-reg", "vscp-info", "vscp-scan"]

KEY128 = "000102030405060708090A0B0C0D0E0F"
HMAC = "1F1E1D1C1B1A191817161514131211100F0E0D0C0B0A09080706050403020100"


def tool(name):
    return os.path.join(BINDIR, name + EXE)


def run(name, *args, timeout=30, env=None):
    return subprocess.run([tool(name)] + list(args), capture_output=True, text=True, timeout=timeout, env=env)


def start_dump(*args):
    """Start vscp-dump and wait until it is connected (it logs that with -v)"""
    proc = subprocess.Popen([tool("vscp-dump"), "-v"] + list(args),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    deadline = time.time() + 30
    while time.time() < deadline:
        line = proc.stderr.readline()
        if not line:
            break
        if "Connected" in line:
            time.sleep(0.2)
            return proc
    proc.kill()
    raise AssertionError("vscp-dump did not connect")


def config(*args):
    """--print-config output as a dict"""
    r = run("vscp-send", "--print-config", *args)
    if r.returncode != 0:
        raise AssertionError("print-config failed: " + r.stderr)
    return json.loads(r.stdout)


class HelpTest(unittest.TestCase):
    def test_help_and_version(self):
        for t in TOOLS:
            r = run(t, "--help")
            self.assertEqual(r.returncode, 0, t)
            self.assertIn("Usage: " + t, r.stdout)
            self.assertIn("--interface", r.stdout)
            self.assertIn("--encryption", r.stdout)
            r = run(t, "--version")
            self.assertEqual(r.returncode, 0, t)
            self.assertIn(t, r.stdout)


class ConfigTest(unittest.TestCase):
    def test_tcpip_defaults(self):
        c = config("-i", "tcp")
        self.assertEqual(c["type"], "tcpip")
        self.assertEqual(c["host"], "tcp://localhost:9598")
        self.assertEqual(c["user"], "admin")
        self.assertEqual(c["password"], "********")  # Masked
        self.assertNotIn("btls", c)

    def test_tcpip_tls_and_encryption(self):
        c = config("-i", "tcpip", "-H", "::1", "-P", "9599", "--tls", "--ca-file", "ca.pem",
                   "-e", "aes128", "-k", KEY128, "--hmac-key", HMAC)
        self.assertEqual(c["host"], "stcp://[::1]:9599")
        self.assertTrue(c["btls"])
        self.assertEqual(c["tls-mode"], "force-tls")
        self.assertEqual(c["cafile"], "ca.pem")
        self.assertTrue(c["binary-mode"])
        self.assertEqual(c["binary-encryption"], 1)
        self.assertEqual(c["binary-key"], "********")

    def test_tcpip_insecure(self):
        c = config("-i", "tcpip", "--tls", "--insecure")
        self.assertFalse(c["bverifypeer"])

    def test_udp_encryption(self):
        c = config("-i", "udp", "-H", "192.0.2.255", "-P", "1234", "-e", "aes256",
                   "-k", KEY128 * 2, "--hmac-key", HMAC, "--require-encryption", "--net-interface", "eth0")
        self.assertEqual(c["ip"], "192.0.2.255")
        self.assertEqual(c["port"], 1234)
        self.assertEqual(c["encryption"], 3)
        self.assertTrue(c["require-encryption"])
        self.assertEqual(c["interface"], "eth0")

    def test_websocket(self):
        c = config("-i", "ws2", "-H", "srv", "--tls", "-k", KEY128)
        self.assertEqual(c["url"], "wss://srv:8884/ws2")
        c = config("-i", "ws1")
        self.assertEqual(c["url"], "ws://localhost:8884/ws1")

    def test_mqtt(self):
        c = config("-i", "mqtt", "-H", "broker", "--tls", "--ca-path", "/etc/ssl/certs",
                   "--subscribe", "a/#", "--publish", "b/{{class}}", "--qos", "1", "--mqtt-format", "string")
        self.assertEqual(c["port"], 8883)
        self.assertEqual(c["tls"]["capath"], "/etc/ssl/certs")
        self.assertEqual(c["subscribe"][0]["topic"], "a/#")
        self.assertEqual(c["publish"][0]["topic"], "b/{{class}}")
        self.assertEqual(c["publish"][0]["qos"], 1)
        self.assertEqual(c["publish"][0]["format"], "string")

    def test_canal_level2(self):
        c = config("-i", "canal", "--driver", "/x/drv.so", "--driver-config", "a;b", "--flags", "3")
        self.assertEqual((c["path"], c["config"], c["flags"]), ("/x/drv.so", "a;b", 3))
        c = config("-i", "level2", "--driver", "/x/l2.so", "--driver-config", "/x/l2.json")
        self.assertEqual((c["path-driver"], c["path-config"]), ("/x/l2.so", "/x/l2.json"))

    def test_config_file_and_set(self):
        path = os.path.join(BINDIR, "test-cfg.json")
        with open(path, "w") as f:
            json.dump({"interface": {"type": "udp", "ip": "10.0.0.255", "port": 4000}}, f)
        try:
            c = config("-c", path, "-P", "5000", "--set", "response-timeout=99", "--set", "x=text")
            self.assertEqual(c["type"], "udp")
            self.assertEqual(c["ip"], "10.0.0.255")
            self.assertEqual(c["port"], 5000)  # Command line wins
            self.assertEqual(c["response-timeout"], 99)
            self.assertEqual(c["x"], "text")
        finally:
            os.remove(path)

    def test_password_from_environment(self):
        env = dict(os.environ, VSCP_PASSWORD="fromenv")
        r = run("vscp-send", "-i", "tcpip", "-u", "bob", "--print-config", env=env)
        self.assertEqual(json.loads(r.stdout)["password"], "********")


class ErrorTest(unittest.TestCase):
    def expect(self, code, *args, tool_name="vscp-send"):
        r = run(tool_name, *args)
        self.assertEqual(r.returncode, code, " ".join(args) + "\n" + r.stdout + r.stderr)
        return r

    def test_usage_errors(self):
        self.expect(2, "--no-such-option")
        self.expect(2, "-i", "nosuch", "20", "3")
        self.expect(2, "20", "3")  # No interface
        self.expect(2, "-i", "udp", "--tls", "--print-config")
        self.expect(2, "-i", "mqtt", "--tls", "--print-config")  # No CA
        self.expect(2, "-i", "udp", "-e", "aes128", "-k", "0011", "--print-config")  # Short key
        self.expect(2, "-i", "tcpip", "-e", "aes128", "-k", KEY128, "--print-config")  # No HMAC key
        self.expect(2, "-i", "udp", "20", "3", "1", "2", "3", "4", "5", "6", "7", "8", "9")  # > 8 bytes
        self.expect(2, "-i", "udp", "20", "3", "256")  # Not a byte
        self.expect(2, "-i", "udp", "-C", "0", tool_name="vscp-gen")  # Protocol class
        self.expect(2, "-i", "udp", "read", "0", tool_name="vscp-reg")  # No node
        self.expect(2, "-i", "udp", "-N", "1", "read", "0xff", "2", tool_name="vscp-reg")
        self.expect(2, "-i", "udp", "-N", "1", "erase", tool_name="vscp-reg")

    def test_connection_error(self):
        # Nothing listens on this port
        r = self.expect(3, "-i", "tcpip", "-H", "127.0.0.1", "-P", "1", "--connect-timeout", "2000", "20", "3")
        self.assertIn("connect", r.stderr.lower())


@unittest.skipUnless(platform.system() == "Linux", "loopback broadcast test runs on Linux")
class UdpLoopbackTest(unittest.TestCase):
    PORT = "39598"

    def roundtrip(self, *security):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT] + list(security)
        dump = start_dump(*common, "-f", "json", "-n", "2", "--idle", "20000")
        r1 = run("vscp-send", *common, "-p", "5", "20", "3", "0", "1", "2")
        r2 = run("vscp-send", *common, "-E", "0,10,6,0,,0,-,0x89,0x02,0x00,0xe6")
        out, err = dump.communicate(timeout=60)
        self.assertEqual(r1.returncode, 0, r1.stderr)
        self.assertEqual(r2.returncode, 0, r2.stderr)
        self.assertEqual(dump.returncode, 0, err)
        events = [json.loads(line) for line in out.splitlines()]
        self.assertEqual([(e["class"], e["type"], e["data"]) for e in events],
                         [(20, 3, [0, 1, 2]), (10, 6, [0x89, 2, 0, 0xE6])])
        self.assertEqual((events[0]["head"] >> 5) & 7, 5)  # Priority

    def test_plain(self):
        self.roundtrip()

    def test_encrypted(self):
        self.roundtrip("-e", "aes128", "-k", KEY128, "--hmac-key", HMAC, "--require-encryption")

    def test_gen(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        dump = start_dump(*common, "-f", "csv", "-t", "n", "-n", "20", "--idle", "20000")
        r = run("vscp-gen", *common, "-n", "20", "-g", "5", "--seed", "7")
        out, err = dump.communicate(timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        rows = [line.split(",") for line in out.splitlines()[1:]]
        self.assertEqual(len(rows), 20, err)
        for row in rows:
            self.assertNotIn(int(row[1]), (0, 512, 1024))  # No protocol classes
            self.assertLessEqual(int(row[5]), 8)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bindir", default=".")
    args, rest = parser.parse_known_args()
    BINDIR = os.path.abspath(args.bindir)
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
