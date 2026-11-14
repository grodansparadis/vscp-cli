// vscp-dump.cpp
//
// Show VSCP events received on any VSCP interface.
//
//   vscp-dump -i udp
//   vscp-dump -i tcpip -H server -f json --class 10 --count 20
//   vscp-dump -i multicast -f string > events.txt   (replay with vscp-send --file)
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
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

int
main(int argc, char **argv)
{
  ConnectionOptions conn;

  OutputFormat format = OutputFormat::TEXT;
  TimeMode timeMode   = TimeMode::ABSOLUTE;
  bool timeSet        = false;
  uint64_t count      = 0;
  uint32_t idle       = 0;
  uint32_t duration   = 0;
  std::string logFile;
  std::set<uint16_t> classes;
  std::set<uint16_t> types;
  std::string filterStr, maskStr;
  std::vector<uint8_t> guidFilter;
  bool quiet = false;

  ArgParser p("vscp-dump", "[OPTIONS]", "Show received VSCP events, one per line, until Ctrl+C.");

  p.group("Output");

  p.option("format", 'f', "FMT",
           "text (default), string, json, xml or csv. string and\n"
           "json lines can be sent again with vscp-send --file.",
           [&](const std::string &v, std::string &) { return parseOutputFormat(v, format); });

  p.option("time", 't', "MODE",
           "Time stamp in text/csv output: a = absolute local time\n"
           "(default), d = delta, z = from the first event, n = none",
           [&](const std::string &v, std::string &) {
             timeSet = true;
             return parseTimeMode(v, timeMode);
           });

  p.option("log", 'l', "FILE", "Also append the output to FILE", [&](const std::string &v, std::string &) {
    logFile = v;
    return true;
  });

  p.flag("quiet", 'q', "No output on stdout (use with --log or --count)", [&]() { quiet = true; });

  p.group("Selection");

  p.option("class", 'C', "LIST", "Only these classes (e.g. 10,20). Repeatable.",
           [&](const std::string &v, std::string &) {
             for (const auto &s : split(v, ", ")) {
               uint64_t n;
               if (!parseUnsigned(s, 0xffff, n)) {
                 return false;
               }
               classes.insert((uint16_t) n);
             }
             return true;
           });

  p.option("type", 'T', "LIST", "Only these types (e.g. 3,4). Repeatable.", [&](const std::string &v, std::string &) {
    for (const auto &s : split(v, ", ")) {
      uint64_t n;
      if (!parseUnsigned(s, 0xffff, n)) {
        return false;
      }
      types.insert((uint16_t) n);
    }
    return true;
  });

  p.option("from", 0, "GUID", "Only events from this GUID", [&](const std::string &v, std::string &) {
    return parseHexString(v, guidFilter) && (16 == guidFilter.size());
  });

  p.option("filter", 0, "FILTER", "VSCP filter priority,class,type,GUID", [&](const std::string &v, std::string &) {
    filterStr = v;
    return true;
  });

  p.option("mask", 0, "MASK", "VSCP mask priority,class,type,GUID", [&](const std::string &v, std::string &) {
    maskStr = v;
    return true;
  });

  p.group("Stop");

  p.option("count", 'n', "N", "Exit after N events", [&](const std::string &v, std::string &) {
    return parseUnsigned(v, UINT64_MAX, count);
  });

  p.option("idle", 0, "MS", "Exit when no event has arrived for MS ms", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 0xffffffff, n)) {
      return false;
    }
    idle = (uint32_t) n;
    return true;
  });

  p.option("duration", 0, "MS", "Exit after MS ms", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 0xffffffff, n)) {
      return false;
    }
    duration = (uint32_t) n;
    return true;
  });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-dump -i udp\n"
           "  vscp-dump -i multicast -e aes128 -k 000102030405060708090A0B0C0D0E0F --require-encryption\n"
           "  vscp-dump -i tcpip -H server --tls --ca-file ca.pem -C 10 -f json\n"
           "  vscp-dump -i socketcan --device can0 -t d\n"
           "  vscp-dump -i mqtt -H broker --subscribe 'vscp/#' -f string -l events.txt\n"
           "\n"
           "Exit status is 0, or 4 when --count events did not arrive before --idle/--duration.");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-dump: %s\n", error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  if (!pos.empty()) {
    fprintf(stderr, "vscp-dump: unexpected argument '%s'\n", pos[0].c_str());
    return EXIT_USAGE;
  }
  finishConnectionOptions(conn);

  int rc;
  if (handlePrintConfig(conn, rc)) {
    return rc;
  }

  if (!timeSet && (OutputFormat::CSV == format)) {
    timeMode = TimeMode::ABSOLUTE;
  }

  vscpEventFilter filter;
  memset(&filter, 0, sizeof(filter)); // Everything passes
  bool useFilter = !filterStr.empty() || !maskStr.empty();
  if ((!filterStr.empty() && !vscp_readFilterFromString(&filter, filterStr)) ||
      (!maskStr.empty() && !vscp_readMaskFromString(&filter, maskStr))) {
    fprintf(stderr, "vscp-dump: invalid --filter/--mask (use priority,class,type,GUID)\n");
    return EXIT_USAGE;
  }

  FILE *log = nullptr;
  if (!logFile.empty()) {
    log = fopen(logFile.c_str(), "a");
    if (nullptr == log) {
      fprintf(stderr, "vscp-dump: unable to open %s\n", logFile.c_str());
      return EXIT_USAGE;
    }
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-dump: %s\n", error.c_str());
    if (log) {
      fclose(log);
    }
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  EventFormatter fmt(format, timeMode);
  std::string header = fmt.header();
  if (!header.empty()) {
    if (!quiet) {
      printf("%s\n", header.c_str());
    }
    if (log) {
      fprintf(log, "%s\n", header.c_str());
    }
  }

  uint64_t received  = 0;
  uint64_t start     = msNow();
  uint64_t lastEvent = start;
  int result         = EXIT_OK;
  int errors         = 0;

  while (g_run) {
    uint64_t now = msNow();
    if (duration && ((now - start) >= duration)) {
      break;
    }
    if (idle && ((now - lastEvent) >= idle)) {
      break;
    }

    vscpEventEx ex;
    memset(&ex, 0, sizeof(ex));
    int rv = client->receiveBlocking(ex, 100);
    if (VSCP_ERROR_SUCCESS != rv) {
      if ((VSCP_ERROR_TIMEOUT == rv) || (VSCP_ERROR_FIFO_EMPTY == rv) || (VSCP_ERROR_RCV_EMPTY == rv)) {
        continue;
      }
      if (!client->isConnected()) {
        fprintf(stderr, "vscp-dump: connection lost\n");
        result = EXIT_CONNECTION;
        break;
      }
      // Some clients report other codes when nothing is waiting
      if (++errors > 100) {
        fprintf(stderr, "vscp-dump: receive failed: %s\n", errorText(rv).c_str());
        result = EXIT_ERROR;
        break;
      }
      sleepMs(10);
      continue;
    }
    errors = 0;

    if (!classes.empty() && !classes.count(ex.vscp_class)) {
      continue;
    }
    if (!types.empty() && !types.count(ex.vscp_type)) {
      continue;
    }
    if (!guidFilter.empty() && memcmp(ex.GUID, guidFilter.data(), 16)) {
      continue;
    }
    if (useFilter && !vscp_doLevel2FilterEx(&ex, &filter)) {
      continue;
    }

    lastEvent        = msNow();
    std::string line = fmt.format(ex);
    if (!quiet) {
      printf("%s\n", line.c_str());
      fflush(stdout);
    }
    if (log) {
      fprintf(log, "%s\n", line.c_str());
      fflush(log);
    }

    if (count && (++received >= count)) {
      break;
    }
  }

  if (count && (received < count) && (EXIT_OK == result) && g_run) {
    result = EXIT_TIMEOUT;
  }

  closeClient(client);
  if (log) {
    fclose(log);
  }
  return result;
}
