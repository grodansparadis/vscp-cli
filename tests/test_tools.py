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
import struct
import subprocess
import sys
import tempfile
import time
import unittest

BINDIR = "."
EXE = ".exe" if os.name == "nt" else ""
TOOLS = ["vscp-send", "vscp-dump", "vscp-gen", "vscp-reg", "vscp-info", "vscp-scan"]

KEY128 = "000102030405060708090A0B0C0D0E0F"
HMAC = "1F1E1D1C1B1A191817161514131211100F0E0D0C0B0A09080706050403020100"

ZERO_GUID = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00"
FF_GUID = "FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF"
EXTENDED_GUID = "FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:01:02"
GUID_FORMS = [
    ("", ZERO_GUID),
    ("-", ZERO_GUID),
    ("-:", ZERO_GUID),
    ("::", FF_GUID),
    ("*:", FF_GUID),
    ("::01:02", EXTENDED_GUID),
    ("*:0102", EXTENDED_GUID),
    ("{::01,02}", EXTENDED_GUID),
    ("-:01:02", "00:00:00:00:00:00:00:00:00:00:00:00:00:00:01:02"),
    ("01:02::FE:FF", "01:02:00:00:00:00:00:00:00:00:00:00:00:00:FE:FF"),
    ("01:02::", "01:02:00:00:00:00:00:00:00:00:00:00:00:00:00:00"),
    ("{::0102:03aa:44:01:30}", "FF:FF:FF:FF:FF:FF:FF:FF:FF:01:02:03:AA:44:01:30"),
    ("FFEEDDCC-BBAA-9988-7766-554433221100", "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00"),
    ("ffeeddccbbaa99887766554433221100", "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00"),
    ("0" * 30 + "1", "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:10"),
    ("FFEEDD:CCBBAA99-8877,665544332211:00", "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00"),
    ("FF EE DD CC BB AA 99 88 77 66 55 44 33 22 11 00",
     "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00"),
    ("::12345", "FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:12:34:50"),
]


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

    def test_guid_options_accept_supported_forms(self):
        options = [("vscp-send", "--guid"), ("vscp-gen", "--guid"), ("vscp-dump", "--from"),
                   ("vscp-reg", "--node"), ("vscp-info", "--node"),
                   ("vscp-reg", "--via"), ("vscp-info", "--via"), ("vscp-scan", "--via")]
        for value, expected in GUID_FORMS:
            with self.subTest(value=value, option="--if-guid"):
                c = config("-i", "level2", "--driver", "/x/l2.so", "--if-guid", value)
                self.assertEqual(c["guid"], expected)
            for name, option in options:
                with self.subTest(value=value, tool=name, option=option):
                    r = run(name, "-i", "udp", "--print-config", option, value)
                    self.assertEqual(r.returncode, 0, r.stderr)
            for option in ("--filter", "--mask"):
                with self.subTest(value=value, option=option):
                    r = run("vscp-dump", "-i", "udp", "--print-config", option, "0,20,3," + value)
                    self.assertEqual(r.returncode, 0, r.stderr)

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

    def test_stored_arguments(self):
        path = os.path.join(BINDIR, "test-profile.json")
        with open(path, "w") as f:
            json.dump({"interface": {"type": "udp", "ip": "10.0.0.255"},
                       "args": ["--response-timeout", 1234],
                       "vscp-send": ["-P", "4000", "--print-config"],
                       "vscp-dump": ["-P", "5000"]}, f)
        try:
            # args and the vscp-send list are used; the command line wins
            c = config("-c", path)
            self.assertEqual((c["ip"], c["port"]), ("10.0.0.255", 4000))
            self.assertNotIn("args", c)
            c = config("-c", path, "-P", "4500")
            self.assertEqual(c["port"], 4500)
            # Another tool gets its own list
            r = run("vscp-dump", "-c", path, "--print-config")
            self.assertEqual(json.loads(r.stdout)["port"], 5000)
            # Positional arguments come from the file when none are given
            with open(path, "w") as f:
                json.dump({"interface": {"type": "udp"}, "vscp-send": ["20", "3", "1", "2", "3", "4", "5", "6", "7", "8", "9"]}, f)
            r = run("vscp-send", "-c", path)
            self.assertEqual(r.returncode, 2)  # 9 data bytes from the file are rejected
            self.assertIn("at most 8 data bytes", r.stderr)
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

    def test_send_positional_event_errors(self):
        self.expect(2, "-i", "udp", "96,20,3")
        self.expect(2, "-i", "udp", "96,20,3,0,,0,-,1,2,3,4,5,6,7,8,9")
        self.expect(2, "-i", "udp", "96,20,3,0,,0,-,1", "2")

    def test_invalid_guid_inputs(self):
        invalid = ["not-a-guid", "::xyz", "-:01:zz", "01:02::GG", "01::02::03",
                   "{::01", "00:01", "00000000000000000000000000000000extra"]
        options = [("vscp-send", "--guid"), ("vscp-gen", "--guid"), ("vscp-dump", "--from"),
                   ("vscp-reg", "--node"), ("vscp-info", "--node"),
                   ("vscp-reg", "--via"), ("vscp-info", "--via"), ("vscp-scan", "--via"),
                   ("vscp-send", "--if-guid")]
        for value in invalid:
            for name, option in options:
                with self.subTest(value=value, tool=name, option=option):
                    self.expect(2, "-i", "udp", option, value, "--print-config", tool_name=name)
            for option in ("--filter", "--mask"):
                with self.subTest(value=value, option=option):
                    self.expect(2, "-i", "udp", option, "0,20,3," + value, "--print-config",
                                tool_name="vscp-dump")
        for event in ("96,20,3,0,,0,::xyz,1",
                      '{"class":20,"type":3,"guid":"::xyz"}',
                      '{"class":20,"type":3,"guid":123}',
                      '<event class="20" type="3" guid="::xyz"/>',
                      "96,20,3,0,,0,{::01,02,1"):
            with self.subTest(event=event):
                r = self.expect(2, "-i", "tcpip", "-H", "127.0.0.1", "-P", "1", "-E", event)
                self.assertIn("GUID", r.stderr)

    def test_gen_invalid_pairs_and_sizes(self):
        cases = [
            ("-C", "3"),                           # Undefined class
            ("-C", "20", "-T", "65535"),            # Undefined type in a defined class
            ("-C", "10", "-T", "47"),               # Reserved measurement type
            ("-C", "20", "-L", "9", "--level2"),    # Level II flag cannot enlarge Level I
            ("-C", "20", "-D", ":000102030405060708"),
            ("-C", "532", "-L", "15"),              # Missing encapsulated GUID
            ("-C", "532", "-L", "25"),              # Encapsulated payload exceeds 8 bytes
            ("-C", "1040", "-L", "513"),
            ("-C", "1040", "-D", ":" + "00" * 513),
            ("-C", "20", "-L", "3", "-D", "1,2"),  # Conflicting fixed size
            ("--measurements", "-C", "20"),
            ("--measurements", "-L", "4"),
            ("--measurements", "-D", "i"),
        ]
        for args in cases:
            with self.subTest(args=args):
                self.expect(2, "-i", "tcpip", "-H", "127.0.0.1", "-P", "1",
                            *args, tool_name="vscp-gen")


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

    def test_send_positional_event(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        guid = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:42"
        dump = start_dump(*common, "-f", "json", "-n", "3", "--idle", "2000")
        try:
            preserved = run("vscp-send", *common, "64,10,6,0,,0," + guid + ",0x80,0x82,1,2")
            self.assertEqual(preserved.returncode, 0, preserved.stderr)
            r = run("vscp-send", *common, "-p", "5", "-g", guid,
                    "96,20,3,0,,0,-,0,0x01,2", "-E", "96,20,4,0,,0,-")
            self.assertEqual(r.returncode, 0, r.stderr)
            out, err = dump.communicate(timeout=10)
            self.assertEqual(dump.returncode, 0, err)
            events = [json.loads(line) for line in out.splitlines()]
            self.assertEqual([(e["class"], e["type"], e["data"]) for e in events],
                             [(10, 6, [0x80, 0x82, 1, 2]), (20, 3, [0, 1, 2]), (20, 4, [])])
            self.assertEqual((events[0]["head"] >> 5) & 7, 2)
            self.assertEqual(events[0]["guid"].upper(), guid)
            for event in events[1:]:
                self.assertEqual((event["head"] >> 5) & 7, 5)
                self.assertEqual(event["guid"].upper(), guid)
        finally:
            if dump.poll() is None:
                dump.kill()
                dump.communicate()

    def test_guid_event_and_generator_options(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        for value, expected in GUID_FORMS:
            with self.subTest(value=value, tool="vscp-send"):
                r = run("vscp-send", *common, "--echo", "--guid", value, "20", "3")
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertIn(expected, r.stdout)
            with self.subTest(value=value, tool="vscp-gen"):
                r = run("vscp-gen", *common, "--echo", "--guid", value, "-C", "20", "-T", "3",
                        "-L", "0", "-n", "1", "-g", "0")
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertIn(expected, r.stdout)

    def test_guid_event_formats(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        args = []
        expected = []
        for value, guid in GUID_FORMS:
            string_guid = ("{" + value + "}") if "," in value and not value.startswith("{") else value
            formats = [
                "96,20,3,0,,0," + string_guid + ",0xAA,0xBB",
                json.dumps({"class": 20, "type": 3, "guid": value, "data": [0xAA, 0xBB]}),
                '<event class="20" type="3" guid="' + value + '" data="0xAA,0xBB"/>',
            ]
            for event in formats:
                args.extend(["-E", event])
                expected.append(guid)
        r = run("vscp-send", *common, "--echo", *args)
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = r.stdout.splitlines()
        self.assertEqual(len(lines), len(expected))
        for line, guid in zip(lines, expected):
            self.assertIn(guid + "  [2] AA BB", line)

        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "events.txt")
            with open(path, "w") as f:
                f.write("96,20,3,0,,0,{::01,02},1\n")
            r = run("vscp-send", *common, "--echo", "96,20,3,0,,0,-:01:02,2", "--file", path)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("00:00:00:00:00:00:00:00:00:00:00:00:00:00:01:02  [1] 02", r.stdout)
            self.assertIn(EXTENDED_GUID + "  [1] 01", r.stdout)

        r = run("vscp-send", *common, "--echo",
                "-E", "96,20,3,0,,0,::01,02",
                "-E", '{"vscpClass":20,"vscpType":3,"vscpGuid":"-:01:02","vscpData":[3]}',
                "-E", '<event class="20" type="3" guid="&#42;&#58;0102" data="4"/>')
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = r.stdout.splitlines()
        self.assertEqual(len(lines), 3)
        self.assertIn("FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:01  [1] 02", lines[0])
        self.assertIn("00:00:00:00:00:00:00:00:00:00:00:00:00:00:01:02  [1] 03", lines[1])
        self.assertIn(EXTENDED_GUID + "  [1] 04", lines[2])

    def test_guid_event_formats_on_wire(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        dump = start_dump(*common, "-f", "json", "-n", "4", "--idle", "2000")
        try:
            r = run("vscp-send", *common, "-d", "5", "96,20,3,0,,0,{::01,02},1",
                    "-E", "96,20,3,0,,0,FFEEDDCC-BBAA-9988-7766-554433221100,2",
                    "-E", '{"class":20,"type":3,"guid":"*:0102","data":[3]}',
                    "-E", '<event class="20" type="3" guid="-:01:02" data="4"/>')
            self.assertEqual(r.returncode, 0, r.stderr)
            out, err = dump.communicate(timeout=10)
            self.assertEqual(dump.returncode, 0, err)
            events = [json.loads(line) for line in out.splitlines()]
            self.assertEqual([(e["guid"].upper(), e["data"]) for e in events], [
                (EXTENDED_GUID, [1]),
                ("FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00", [2]),
                (EXTENDED_GUID, [3]),
                ("00:00:00:00:00:00:00:00:00:00:00:00:00:00:01:02", [4]),
            ])
        finally:
            if dump.poll() is None:
                dump.kill()
                dump.communicate()

    def test_guid_filters_and_measurement_prefix(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        selectors = [
            ["--from", "*:0102"],
            ["--filter", "0,20,3,::01,02", "--mask", "0,0xffff,0xffff,::"],
        ]
        for selector in selectors:
            with self.subTest(selector=selector):
                dump = start_dump(*common, "-f", "json", "-n", "1", "--idle", "2000", *selector)
                try:
                    wrong = run("vscp-send", *common, "--guid", "::01:03", "20", "3", "2")
                    right = run("vscp-send", *common, "--guid", "{::01,02}", "20", "3", "1")
                    self.assertEqual((wrong.returncode, right.returncode), (0, 0), wrong.stderr + right.stderr)
                    out, err = dump.communicate(timeout=10)
                    self.assertEqual(dump.returncode, 0, err)
                    event = json.loads(out.strip())
                    self.assertEqual((event["guid"].upper(), event["data"]), (EXTENDED_GUID, [1]))
                finally:
                    if dump.poll() is None:
                        dump.kill()
                        dump.communicate()

        dump = start_dump(*common, "-f", "json", "-n", "1", "--idle", "2000", "--from", "::01:02")
        try:
            r = run("vscp-gen", *common, "--measurements", "--guid", "{::01,02}",
                    "-C", "522", "-T", "6", "-n", "1", "-g", "0")
            self.assertEqual(r.returncode, 0, r.stderr)
            out, err = dump.communicate(timeout=10)
            self.assertEqual(dump.returncode, 0, err)
            event = json.loads(out.strip())
            self.assertEqual(event["guid"].upper(), EXTENDED_GUID)
            self.assertEqual(event["data"][:16], list(bytes.fromhex(EXTENDED_GUID.replace(":", ""))))
        finally:
            if dump.poll() is None:
                dump.kill()
                dump.communicate()

    def test_guid_node_and_via_on_wire(self):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        cases = [
            ("vscp-reg", ["-N", "00000000000000000000000000000042", "read", "0x80"], 0x42),
            ("vscp-reg", ["-N", "0" * 30 + "1", "read", "0x80"], 0x10),
            ("vscp-reg", ["-N", "42", "read", "0x80"], 42),
            ("vscp-info", ["-N", "-:42"], 0x42),
            ("vscp-scan", [], 0xFF),
        ]
        for name, args, nickname in cases:
            with self.subTest(tool=name, node=nickname):
                dump = start_dump(*common, "-f", "json", "-n", "1", "--idle", "2000")
                try:
                    r = run(name, *common, "--via", "{::01,02}", "--timeout", "100", *args)
                    self.assertEqual(r.returncode, 4, r.stderr)
                    out, err = dump.communicate(timeout=10)
                    self.assertEqual(dump.returncode, 0, err)
                    event = json.loads(out.strip())
                    self.assertEqual(event["class"], 512)
                    self.assertEqual(event["data"][:16], list(bytes.fromhex(EXTENDED_GUID.replace(":", ""))))
                    self.assertEqual(event["data"][16], nickname)
                finally:
                    if dump.poll() is None:
                        dump.kill()
                        dump.communicate()

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
            self.assertIn(int(row[1]), {1, 2, 10, 11, 12, 13, 14, 15, 20, 30, 40, 50,
                                       60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
                                       70, 71, 72, 73, 74, 85, 86, 87, 88, 89,
                                       90, 95, 100, 102, 110, 120, 206, 212,
                                       506, 508, 509, 510, 511})
            self.assertLessEqual(int(row[5]), 8)

    def generate(self, *args, count=20):
        common = ["-i", "udp", "-H", "127.255.255.255", "-P", self.PORT]
        dump = start_dump(*common, "-f", "json", "-n", str(count), "--idle", "2000")
        try:
            r = run("vscp-gen", *common, "-n", str(count), "-g", "5", "--seed", "7", *args)
            self.assertEqual(r.returncode, 0, r.stderr)
            out, err = dump.communicate(timeout=10)
            self.assertEqual(dump.returncode, 0, err)
            events = [json.loads(line) for line in out.splitlines()]
            self.assertEqual(len(events), count, err)
            return events
        finally:
            if dump.poll() is None:
                dump.kill()
                dump.communicate()

    def test_gen_class_types_and_length_limits(self):
        for cls in (1, 513):
            with self.subTest(cls=cls):
                events = self.generate("-C", str(cls))
                for event in events:
                    self.assertEqual(event["class"], cls)
                    self.assertIn(event["type"], range(14))
                    self.assertGreaterEqual(len(event["data"]), 16 if cls == 513 else 0)
                    self.assertLessEqual(len(event["data"]), 24 if cls == 513 else 8)
        for cls, length in ((20, 8), (532, 16), (532, 24), (1040, 512)):
            with self.subTest(cls=cls, length=length):
                events = self.generate("-C", str(cls), "-T", "0", "-L", str(length), count=2)
                self.assertTrue(all(len(e["data"]) == length for e in events))

    def test_gen_measurements(self):
        guid = "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:42"
        selected = self.generate("-m", "-T", "6")
        self.assertTrue(all(e["class"] in {10, 60, 65, 70} and e["type"] == 6 for e in selected))
        for cls in (10, 60, 65, 70, 522, 572, 577, 582, 1040, 1060):
            with self.subTest(cls=cls):
                events = self.generate("--measurements", "-C", str(cls), "-T", "6", "--guid", guid)
                values = []
                for event in events:
                    self.assertEqual((event["class"], event["type"]), (cls, 6))
                    self.assertEqual(event["guid"].upper(), guid)
                    data = bytes(event["data"])
                    base = cls
                    if 512 <= cls < 1024:
                        self.assertEqual(data[:16], bytes.fromhex(guid.replace(":", "")))
                        data = data[16:]
                        base -= 512
                    if base in (10, 65):
                        if base == 65:
                            self.assertEqual(data[:3], b"\0\0\0")
                            data = data[3:]
                        self.assertEqual(len(data), 4)
                        self.assertEqual(data[:2], b"\x80\x82")
                        value = int.from_bytes(data[2:], "big", signed=True) / 100
                    elif base in (60, 70):
                        value, = struct.unpack(">d" if base == 60 else ">f", data)
                    else:
                        self.assertEqual(data[:4], b"\0\0\0\0")
                        if base == 1040:
                            value = float(data[4:].rstrip(b"\0").decode("ascii"))
                        else:
                            value, = struct.unpack(">d", data[4:])
                    self.assertGreaterEqual(value, 0)
                    self.assertLessEqual(value, 100)
                    values.append(value)
                self.assertGreater(len(set(values)), 1)

        first = self.generate("--measurements", "--level2")
        second = self.generate("--measurements", "--level2")
        sequence = lambda events: [(e["class"], e["type"], e["data"]) for e in events]
        self.assertEqual(sequence(first), sequence(second))
        self.assertTrue(all(e["class"] in {10, 60, 65, 70, 522, 572, 577, 582, 1040, 1060}
                            and e["type"] not in (47, 48) for e in first))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bindir", default=".")
    args, rest = parser.parse_known_args()
    BINDIR = os.path.abspath(args.bindir)
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
