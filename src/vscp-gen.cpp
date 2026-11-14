// vscp-gen.cpp
//
// Generate VSCP events (random or with fixed parts) for testing, in the
// spirit of cangen from can-utils.
//
//   vscp-gen -i udp                         random Level I events every 200 ms
//   vscp-gen -i tcpip -C 20 -T 3 -n 100 -g 10
//   vscp-gen -i multicast --level2 -L r
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <cli-args.h>
#include <cli-connection.h>
#include <cli-event.h>
#include <cli-util.h>

#include <guid.h>
#include <vscp-client-base.h>
#include <vscphelper.h>

using namespace vscpcli;

// A value that is fixed, random or (for data) incrementing
struct Spec {
  enum Mode { FIXED, RANDOM, INCREMENT } mode = RANDOM;
  uint64_t value                               = 0;
};

static bool
parseSpec(const std::string &v, uint64_t max, Spec &spec, bool allowIncrement = false)
{
  std::string s = lower(v);
  if ("r" == s) {
    spec.mode = Spec::RANDOM;
    return true;
  }
  if (allowIncrement && ("i" == s)) {
    spec.mode = Spec::INCREMENT;
    return true;
  }
  spec.mode = Spec::FIXED;
  return parseUnsigned(v, max, spec.value);
}

// Protocol classes can change the configuration of nodes (registers,
// nickname, bootloader) so random events never use them unless allowed.
static bool
isProtocolClass(uint16_t cls)
{
  return (VSCP_CLASS1_PROTOCOL == cls) || (VSCP_CLASS2_LEVEL1_PROTOCOL == cls) || (VSCP_CLASS2_PROTOCOL == cls);
}

int
main(int argc, char **argv)
{
  ConnectionOptions conn;

  uint32_t gap = 200;
  uint64_t count = 0;
  Spec cls, type, prio, len, data;
  std::vector<uint8_t> fixedData;
  bool level2        = false;
  bool allowProtocol = false;
  bool echo          = false;
  uint32_t burst     = 1;
  std::string guid;
  bool guidSet       = false;
  uint64_t seed      = 0;
  bool seedSet       = false;

  ArgParser p("vscp-gen", "[OPTIONS]",
              "Generate VSCP events: random, fixed or incrementing class, type, priority,\n"
              "length and data. Runs until Ctrl+C or --count events have been sent.");

  p.group("Events");

  p.option("class", 'C', "N|r", "Class (default r = random)", [&](const std::string &v, std::string &) {
    return parseSpec(v, 0xffff, cls);
  });

  p.option("type", 'T', "N|r", "Type (default r = random)", [&](const std::string &v, std::string &) {
    return parseSpec(v, 0xffff, type);
  });

  p.option("priority", 'p', "N|r", "Priority 0-7 (default r = random)", [&](const std::string &v, std::string &) {
    return parseSpec(v, 7, prio);
  });

  p.option("length", 'L', "N|r", "Number of data bytes (default r = random)", [&](const std::string &v, std::string &) {
    return parseSpec(v, VSCP_LEVEL2_MAXDATA, len);
  });

  p.option("data", 'D', "BYTES|r|i",
           "Data: fixed bytes (1,2,3 or :010203), r = random\n"
           "(default) or i = incrementing counter (little endian)",
           [&](const std::string &v, std::string &err) {
             std::string s = lower(v);
             if (("r" == s) || ("i" == s)) {
               return parseSpec(v, 0, data, true);
             }
             data.mode = Spec::FIXED;
             fixedData.clear();
             return parseByteList(v, fixedData, err);
           });

  p.option("guid", 0, "GUID", "GUID of the events (default all zero)", [&](const std::string &v, std::string &) {
    std::vector<uint8_t> b;
    if (!parseHexString(v, b) || (16 != b.size())) {
      return false;
    }
    guid    = v;
    guidSet = true;
    return true;
  });

  p.flag("level2", '2', "Random classes up to 1023 and up to 128 data bytes\n(default: Level I classes 0-511, 0-8 bytes)",
         [&]() { level2 = true; });

  p.flag("allow-protocol", 0,
         "Allow random events in the protocol classes 0, 512 and\n"
         "1024. They can change node configuration - careful!",
         [&]() { allowProtocol = true; });

  p.group("Timing");

  p.option("gap", 'g', "MS", "Gap between events in ms (default 200)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 3600000, n)) {
      return false;
    }
    gap = (uint32_t) n;
    return true;
  });

  p.option("burst", 'b', "N", "Events sent back to back each gap (default 1)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 100000, n) || !n) {
      return false;
    }
    burst = (uint32_t) n;
    return true;
  });

  p.option("count", 'n', "N", "Stop after N events (default: run until Ctrl+C)",
           [&](const std::string &v, std::string &) { return parseUnsigned(v, UINT64_MAX, count); });

  p.option("seed", 0, "N", "Random seed (repeatable sequences)", [&](const std::string &v, std::string &) {
    seedSet = true;
    return parseUnsigned(v, UINT64_MAX, seed);
  });

  p.flag("echo", 0, "Print each event as it is sent", [&]() { echo = true; });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-gen -i udp                               random Level I events every 200 ms\n"
           "  vscp-gen -i tcpip -C 10 -T 6 -L 4 -D i -g 10   measurements with a counter\n"
           "  vscp-gen -i socketcan --device vcan0 -g 0 -n 10000   load test\n"
           "  vscp-gen -i multicast --level2 --echo");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-gen: %s\n", error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  if (!pos.empty()) {
    fprintf(stderr, "vscp-gen: unexpected argument '%s'\n", pos[0].c_str());
    return EXIT_USAGE;
  }
  finishConnectionOptions(conn);

  int rc;
  if (handlePrintConfig(conn, rc)) {
    return rc;
  }

  if ((Spec::FIXED == cls.mode) && isProtocolClass((uint16_t) cls.value) && !allowProtocol) {
    fprintf(stderr, "vscp-gen: class %u is a protocol class, add --allow-protocol if you really mean it\n",
            (unsigned) cls.value);
    return EXIT_USAGE;
  }

  // Data length limits
  const uint64_t maxLen = level2 ? 128 : VSCP_LEVEL1_MAXDATA;
  if ((Spec::FIXED == len.mode) && (Spec::FIXED != data.mode) && (len.value > VSCP_LEVEL1_MAXDATA) && !level2 &&
      ((Spec::FIXED != cls.mode) || (cls.value < 512))) {
    fprintf(stderr, "vscp-gen: Level I events have at most 8 data bytes (use --level2)\n");
    return EXIT_USAGE;
  }
  if ((Spec::FIXED == data.mode) && (fixedData.size() > VSCP_LEVEL2_MAXDATA)) {
    fprintf(stderr, "vscp-gen: too much data\n");
    return EXIT_USAGE;
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-gen: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  std::mt19937_64 rng(seedSet ? seed : std::random_device{}());
  auto rnd = [&](uint64_t lo, uint64_t hi) { return std::uniform_int_distribution<uint64_t>(lo, hi)(rng); };

  EventFormatter fmt(OutputFormat::TEXT, TimeMode::NONE);
  uint64_t sent    = 0;
  uint64_t counter = 0;
  int failed       = 0;

  while (g_run && (!count || (sent < count))) {
    for (uint32_t b = 0; g_run && (b < burst) && (!count || (sent < count)); b++) {
      vscpEventEx ex;
      initEvent(ex, (uint8_t) ((Spec::FIXED == prio.mode) ? prio.value : rnd(0, 7)));

      if (Spec::FIXED == cls.mode) {
        ex.vscp_class = (uint16_t) cls.value;
      }
      else {
        do {
          ex.vscp_class = (uint16_t) rnd(0, level2 ? 1023 : 511);
        } while (!allowProtocol && isProtocolClass(ex.vscp_class));
      }
      ex.vscp_type = (uint16_t) ((Spec::FIXED == type.mode) ? type.value : rnd(0, 255));

      if (guidSet) {
        vscp_setEventExGuidFromString(&ex, guid);
      }

      // Level II classes 512-1023 are Level I events with the GUID first
      bool level1Over2 = (ex.vscp_class >= 512) && (ex.vscp_class < 1024);
      size_t limit     = (ex.vscp_class < 512) ? VSCP_LEVEL1_MAXDATA : maxLen;

      if (Spec::FIXED == data.mode) {
        ex.sizeData = (uint16_t) fixedData.size();
        if (!fixedData.empty()) {
          memcpy(ex.data, fixedData.data(), fixedData.size());
        }
      }
      else {
        size_t n = (Spec::FIXED == len.mode) ? (size_t) len.value : (size_t) rnd(0, limit);
        if (level1Over2 && (Spec::FIXED != len.mode)) {
          n = 16 + (size_t) rnd(0, VSCP_LEVEL1_MAXDATA);
        }
        ex.sizeData = (uint16_t) n;
        for (size_t i = 0; i < n; i++) {
          if (Spec::INCREMENT == data.mode) {
            ex.data[i] = (i < 8) ? (uint8_t) (counter >> (8 * i)) : 0;
          }
          else {
            ex.data[i] = (uint8_t) rnd(0, 255);
          }
        }
      }
      counter++;

      vscp_setEventExDateTimeBlockToNow(&ex);
      int rv = client->send(ex);
      if (VSCP_ERROR_SUCCESS != rv) {
        fprintf(stderr, "vscp-gen: send failed: %s\n", errorText(rv).c_str());
        if (++failed >= 10) {
          fprintf(stderr, "vscp-gen: giving up\n");
          g_run = false;
        }
        continue;
      }
      failed = 0;
      sent++;
      if (echo) {
        printf("%s\n", fmt.format(ex).c_str());
        fflush(stdout);
      }
    }
    if (gap) {
      sleepMs(gap);
    }
  }

  if ("mqtt" == conn.iface) {
    sleepMs(500); // Let queued messages get out
  }
  closeClient(client);
  if (conn.verbose) {
    fprintf(stderr, "vscp-gen: %llu event(s) sent\n", (unsigned long long) sent);
  }
  return failed ? EXIT_ERROR : EXIT_OK;
}
