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

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
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
  enum Mode { SPEC_FIXED, SPEC_RANDOM, SPEC_INCREMENT } mode = SPEC_RANDOM;
  uint64_t value                               = 0;
};

static bool
parseSpec(const std::string &v, uint64_t max, Spec &spec, bool allowIncrement = false)
{
  std::string s = lower(v);
  if ("r" == s) {
    spec.mode = Spec::SPEC_RANDOM;
    return true;
  }
  if (allowIncrement && ("i" == s)) {
    spec.mode = Spec::SPEC_INCREMENT;
    return true;
  }
  spec.mode = Spec::SPEC_FIXED;
  return parseUnsigned(v, max, spec.value);
}

// Protocol classes can change the configuration of nodes (registers,
// nickname, bootloader) so random events never use them unless allowed.
static bool
isProtocolClass(uint16_t cls)
{
  return (VSCP_CLASS1_PROTOCOL == cls) || (VSCP_CLASS2_LEVEL1_PROTOCOL == cls) || (VSCP_CLASS2_PROTOCOL == cls);
}

static bool
isMeasurementClass(uint16_t cls)
{
  if ((cls >= 512) && (cls < 1024)) {
    cls -= 512;
  }
  return (VSCP_CLASS1_MEASUREMENT == cls) || (VSCP_CLASS1_MEASUREMENT64 == cls) ||
         (VSCP_CLASS1_MEASUREZONE == cls) || (VSCP_CLASS1_MEASUREMENT32 == cls) ||
         (VSCP_CLASS2_MEASUREMENT_STR == cls) || (VSCP_CLASS2_MEASUREMENT_FLOAT == cls);
}

static size_t
minLength(uint16_t cls)
{
  return ((cls >= 512) && (cls < 1024)) ? 16 : 0;
}

static size_t
maxLength(uint16_t cls)
{
  return (cls < 512) ? VSCP_LEVEL1_MAXDATA : ((cls < 1024) ? 16 + VSCP_LEVEL1_MAXDATA : VSCP_LEVEL2_MAXDATA);
}

static bool
makeMeasurement(vscpEventEx &ex, uint64_t sample)
{
  const double value = static_cast<double>(sample) / 100;
  if (VSCP_CLASS2_MEASUREMENT_STR == ex.vscp_class) {
    return vscp_makeLevel2StringMeasurementEventEx(&ex, ex.vscp_type, value, 0, 0, 0, 0);
  }
  if (VSCP_CLASS2_MEASUREMENT_FLOAT == ex.vscp_class) {
    return vscp_makeLevel2FloatMeasurementEventEx(&ex, ex.vscp_type, value, 0, 0, 0, 0);
  }

  const size_t offset = minLength(ex.vscp_class);
  const uint16_t cls = ex.vscp_class - (offset ? 512 : 0);
  memset(ex.data, 0, offset);
  uint8_t *payload = ex.data + offset;
  if ((VSCP_CLASS1_MEASUREMENT == cls) || (VSCP_CLASS1_MEASUREZONE == cls)) {
    const size_t zone = (VSCP_CLASS1_MEASUREZONE == cls) ? 3 : 0;
    memset(payload, 0, zone);
    // Normalized integer: default unit/sensor, exponent -2, positive signed mantissa.
    payload[zone]     = 0x80;
    payload[zone + 1] = 0x82;
    payload[zone + 2] = static_cast<uint8_t>(sample >> 8);
    payload[zone + 3] = static_cast<uint8_t>(sample);
    ex.sizeData = static_cast<uint16_t>(offset + zone + 4);
  }
  else {
    uint64_t bits = 0;
    const size_t size = (VSCP_CLASS1_MEASUREMENT32 == cls) ? 4 : 8;
    if (4 == size) {
      const float f = static_cast<float>(value);
      uint32_t b;
      memcpy(&b, &f, sizeof(b));
      bits = b;
    }
    else {
      memcpy(&bits, &value, sizeof(bits));
    }
    for (size_t i = 0; i < size; ++i) {
      payload[i] = static_cast<uint8_t>(bits >> (8 * (size - i - 1)));
    }
    ex.sizeData = static_cast<uint16_t>(offset + size);
  }
  return true;
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
  bool measurements  = false;
  bool dataSet       = false;
  uint32_t burst     = 1;
  std::string guid;
  bool guidSet       = false;
  uint64_t seed      = 0;
  bool seedSet       = false;

  ArgParser p("vscp-gen", "[OPTIONS]",
              "Generate defined VSCP class/type pairs with random or fixed fields.\n"
              "Use --measurements for encoded random measurement values.\n"
              "Runs until Ctrl+C or --count events have been sent.");

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
             dataSet = true;
             if (("r" == s) || ("i" == s)) {
               return parseSpec(v, 0, data, true);
             }
             data.mode = Spec::SPEC_FIXED;
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

  p.flag("level2", '2', "Include defined Level II classes up to 1060\n(default: defined Level I classes, at most 8 bytes)",
         [&]() { level2 = true; });

  p.flag("measurements", 'm', "Only random measurement values; select class/type with -C/-T\n"
         "(default units, values 0-100; cannot combine with -L/-D)",
         [&]() { measurements = true; });

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
           "  vscp-gen -i tcpip --measurements -C 10 -T 6 -g 10   random temperatures\n"
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

  if ((Spec::SPEC_FIXED == cls.mode) && isProtocolClass((uint16_t) cls.value) && !allowProtocol) {
    fprintf(stderr, "vscp-gen: class %u is a protocol class, add --allow-protocol if you really mean it\n",
            (unsigned) cls.value);
    return EXIT_USAGE;
  }

  if (measurements && ((Spec::SPEC_FIXED == len.mode) || dataSet)) {
    fprintf(stderr, "vscp-gen: --measurements cannot be combined with --length or --data overrides\n");
    return EXIT_USAGE;
  }
  if ((Spec::SPEC_FIXED == data.mode) && (Spec::SPEC_FIXED == len.mode) && (len.value != fixedData.size())) {
    fprintf(stderr, "vscp-gen: --length does not match the fixed data size\n");
    return EXIT_USAGE;
  }

  struct ClassType {
    uint16_t cls;
    uint16_t type;
  };
  static const ClassType catalog[] = {
#include <vscp-gen-types.inc>
  };
  std::map<uint16_t, std::vector<uint16_t>> candidates;
  for (const auto &entry : catalog) {
    if ((Spec::SPEC_FIXED == cls.mode) ? (entry.cls != cls.value) : (entry.cls > (level2 ? 1060 : 511))) {
      continue;
    }
    if ((!allowProtocol && isProtocolClass(entry.cls)) ||
        ((Spec::SPEC_FIXED == type.mode) && (entry.type != type.value)) ||
        (measurements && !isMeasurementClass(entry.cls))) {
      continue;
    }
    const bool fixedSize = (Spec::SPEC_FIXED == data.mode) || (Spec::SPEC_FIXED == len.mode);
    const size_t size = (Spec::SPEC_FIXED == data.mode) ? fixedData.size() : len.value;
    if (fixedSize && ((size < minLength(entry.cls)) || (size > maxLength(entry.cls)))) {
      continue;
    }
    candidates[entry.cls].push_back(entry.type);
  }
  if (candidates.empty()) {
    fprintf(stderr, "vscp-gen: no defined class/type pairs match the requested options and payload size"
                    " (Level I: 0-8, encapsulated Level I: 16-24, Level II: 0-512 bytes)\n");
    return EXIT_USAGE;
  }
  std::vector<uint16_t> classes;
  for (auto &entry : candidates) {
    auto &types = entry.second;
    std::sort(types.begin(), types.end());
    types.erase(std::unique(types.begin(), types.end()), types.end());
    classes.push_back(entry.first);
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-gen: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  std::mt19937_64 rng(seedSet ? seed : std::random_device{}());
  auto rnd = [&](uint64_t lo, uint64_t hi) { return std::uniform_int_distribution<uint64_t>(lo, hi)(rng); };

  EventFormatter fmt(OutputFormat::FMT_TEXT, TimeMode::TIME_NONE);
  uint64_t sent    = 0;
  uint64_t counter = 0;
  int failed       = 0;

  while (g_run && (!count || (sent < count))) {
    for (uint32_t b = 0; g_run && (b < burst) && (!count || (sent < count)); b++) {
      vscpEventEx ex;
      initEvent(ex, (uint8_t) ((Spec::SPEC_FIXED == prio.mode) ? prio.value : rnd(0, 7)));

      ex.vscp_class = classes[rnd(0, classes.size() - 1)];
      const auto &types = candidates.at(ex.vscp_class);
      ex.vscp_type = types[rnd(0, types.size() - 1)];

      if (measurements) {
        if (!makeMeasurement(ex, rnd(0, 10000))) {
          fprintf(stderr, "vscp-gen: could not encode measurement\n");
          closeClient(client);
          return EXIT_ERROR;
        }
      }
      else if (Spec::SPEC_FIXED == data.mode) {
        ex.sizeData = (uint16_t) fixedData.size();
        if (!fixedData.empty()) {
          memcpy(ex.data, fixedData.data(), fixedData.size());
        }
      }
      else {
        size_t n = (Spec::SPEC_FIXED == len.mode) ? (size_t) len.value
                                                : (size_t) rnd(minLength(ex.vscp_class), maxLength(ex.vscp_class));
        ex.sizeData = (uint16_t) n;
        for (size_t i = 0; i < n; i++) {
          if (Spec::SPEC_INCREMENT == data.mode) {
            ex.data[i] = (i < 8) ? (uint8_t) (counter >> (8 * i)) : 0;
          }
          else {
            ex.data[i] = (uint8_t) rnd(0, 255);
          }
        }
      }
      counter++;

      if (guidSet) {
        vscp_setEventExGuidFromString(&ex, guid);
      }
      if (measurements && minLength(ex.vscp_class)) {
        memcpy(ex.data, ex.GUID, 16);
      }

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
