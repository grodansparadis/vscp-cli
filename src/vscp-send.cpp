// vscp-send.cpp
//
// Send VSCP events over any VSCP interface.
//
//   vscp-send -i udp 20 3 0 1 2             CLASS1.INFORMATION, ON
//   vscp-send -i tcpip -E "0,20,3,0,,0,-,0,1,2"
//   vscp-dump -i udp -f string > log.txt; vscp-send -i tcpip --file log.txt
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
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

  std::string guid;
  bool guidSet       = false;
  uint8_t priority   = 3;
  bool prioritySet   = false;
  uint64_t count     = 1;
  uint32_t delay     = 0;
  uint32_t linger    = 0;
  bool lingerSet     = false;
  bool echo          = false;
  bool noCheck       = false;
  std::vector<std::string> events;
  std::vector<std::string> files;

  ArgParser p("vscp-send",
              "[OPTIONS] CLASS TYPE [DATA ...]\n"
              "       vscp-send [OPTIONS] -E EVENT [-E EVENT ...]\n"
              "       vscp-send [OPTIONS] --file FILE|-",
              "Send VSCP events. CLASS, TYPE and the data bytes are decimal or hex (0x..).\n"
              "Data can also be one comma separated list (1,2,0x03) or hex pairs (:010203).");

  p.group("Event");

  p.option("guid", 'g', "GUID", "GUID of the event (default all zero, i.e. the\ninterface GUID is used)",
           [&](const std::string &v, std::string &err) {
             std::vector<uint8_t> b;
             if (("-" != v) && (!parseHexString(v, b) || (16 != b.size()))) {
               err = "use 16 hex bytes, e.g. FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:01";
               return false;
             }
             guid    = v;
             guidSet = true;
             return true;
           });

  p.option("priority", 'p', "0-7", "Priority, 0 is highest (default 3)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 7, n)) {
      return false;
    }
    priority    = (uint8_t) n;
    prioritySet = true;
    return true;
  });

  p.option("event", 'E', "EVENT",
           "Event as a VSCP string\n"
           "(head,class,type,obid,datetime,timestamp,GUID,data...),\n"
           "JSON or XML. Repeatable.",
           [&](const std::string &v, std::string &) {
             events.push_back(v);
             return true;
           });

  p.option("file", 'f', "FILE",
           "Read events from FILE, one per line in any of the\n"
           "--event formats ('-' is stdin). Lines starting with #\n"
           "are skipped. vscp-dump -f string/json writes such files.",
           [&](const std::string &v, std::string &) {
             files.push_back(v);
             return true;
           });

  p.flag("no-check", 0, "Allow more than 8 data bytes for Level I classes (<512)", [&]() { noCheck = true; });

  p.group("Sending");

  p.option("count", 'n', "N", "Send everything N times (default 1, 0 = until Ctrl+C)",
           [&](const std::string &v, std::string &) { return parseUnsigned(v, UINT64_MAX, count); });

  p.option("delay", 'd', "MS", "Delay between events in ms (default 0)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 3600000, n)) {
      return false;
    }
    delay = (uint32_t) n;
    return true;
  });

  p.option("linger", 0, "MS",
           "Wait before disconnecting so queued events get out\n"
           "(default 500 for mqtt, else 0)",
           [&](const std::string &v, std::string &) {
             uint64_t n;
             if (!parseUnsigned(v, 60000, n)) {
               return false;
             }
             linger    = (uint32_t) n;
             lingerSet = true;
             return true;
           });

  p.flag("echo", 0, "Print each event as it is sent", [&]() { echo = true; });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-send -i udp 20 3 0 1 2                 CLASS1.INFORMATION ON, zone 1, subzone 2\n"
           "  vscp-send -i tcpip -H server -u admin --password secret 10 6 0x89 0x81 0x00 0xe6\n"
           "  vscp-send -i tcpip --tls --ca-file ca.pem -E \"0,20,3,0,,0,-,0,1,2\"\n"
           "  vscp-send -i udp -e aes128 -k 000102030405060708090A0B0C0D0E0F 20 3 0 1 2\n"
           "  vscp-send -i mqtt -H broker 20 3 0 1 2\n"
           "  vscp-send -i socketcan --device can0 -n 10 -d 100 20 3 0 1 2\n"
           "  vscp-send -i tcpip --file events.txt");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-send: %s\n", error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  finishConnectionOptions(conn);

  int rc;
  if (handlePrintConfig(conn, rc)) {
    return rc;
  }

  // Collect the events to send
  std::vector<vscpEventEx> list;

  auto addEvent = [&](vscpEventEx &ex, const std::string &what) -> bool {
    if (guidSet) {
      vscp_setEventExGuidFromString(&ex, guid);
    }
    if (prioritySet) {
      vscp_setEventExPriority(&ex, priority);
    }
    if (!noCheck && (ex.vscp_class < 512) && (ex.sizeData > VSCP_LEVEL1_MAXDATA)) {
      fprintf(stderr, "vscp-send: %s: Level I events (class < 512) have at most 8 data bytes (--no-check allows more)\n",
              what.c_str());
      return false;
    }
    list.push_back(ex);
    return true;
  };

  if (!pos.empty()) {
    if (pos.size() < 2) {
      fprintf(stderr, "vscp-send: give CLASS and TYPE (see --help)\n");
      return EXIT_USAGE;
    }
    uint64_t cls, type;
    if (!parseUnsigned(pos[0], 0xffff, cls) || !parseUnsigned(pos[1], 0xffff, type)) {
      fprintf(stderr, "vscp-send: CLASS and TYPE must be numbers 0-65535\n");
      return EXIT_USAGE;
    }
    std::vector<uint8_t> data;
    for (size_t i = 2; i < pos.size(); i++) {
      if (!parseByteList(pos[i], data, error)) {
        fprintf(stderr, "vscp-send: %s\n", error.c_str());
        return EXIT_USAGE;
      }
    }
    if (data.size() > VSCP_LEVEL2_MAXDATA) {
      fprintf(stderr, "vscp-send: at most %d data bytes\n", VSCP_LEVEL2_MAXDATA);
      return EXIT_USAGE;
    }
    vscpEventEx ex;
    initEvent(ex, priority);
    ex.vscp_class = (uint16_t) cls;
    ex.vscp_type  = (uint16_t) type;
    ex.sizeData   = (uint16_t) data.size();
    if (!data.empty()) {
      memcpy(ex.data, data.data(), data.size());
    }
    if (!addEvent(ex, "event")) {
      return EXIT_USAGE;
    }
  }

  for (const auto &str : events) {
    vscpEventEx ex;
    if (!parseEventLine(str, ex, error)) {
      fprintf(stderr, "vscp-send: %s\n", error.c_str());
      return EXIT_USAGE;
    }
    if (!addEvent(ex, str)) {
      return EXIT_USAGE;
    }
  }

  for (const auto &name : files) {
    std::ifstream file;
    std::istream *in = &std::cin;
    if ("-" != name) {
      file.open(name);
      if (!file) {
        fprintf(stderr, "vscp-send: unable to open %s\n", name.c_str());
        return EXIT_USAGE;
      }
      in = &file;
    }
    std::string line;
    int lineno = 0;
    while (std::getline(*in, line)) {
      lineno++;
      line = trim(line);
      if (line.empty() || ('#' == line[0])) {
        continue;
      }
      vscpEventEx ex;
      std::string where = name + ":" + std::to_string(lineno);
      if (!parseEventLine(line, ex, error)) {
        fprintf(stderr, "vscp-send: %s: %s\n", where.c_str(), error.c_str());
        return EXIT_USAGE;
      }
      if (!addEvent(ex, where)) {
        return EXIT_USAGE;
      }
    }
  }

  if (list.empty()) {
    fprintf(stderr, "vscp-send: nothing to send (see --help)\n");
    return EXIT_USAGE;
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-send: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  if (!lingerSet && ("mqtt" == conn.iface)) {
    linger = 500;
  }

  EventFormatter fmt(OutputFormat::TEXT, TimeMode::NONE);
  int failed     = 0;
  uint64_t sent  = 0;
  bool first     = true;

  for (uint64_t round = 0; g_run && (!count || (round < count)); round++) {
    for (auto &ex : list) {
      if (!g_run) {
        break;
      }
      if (!first && delay) {
        sleepMs(delay);
      }
      first = false;

      vscpEventEx out = ex;
      int rv          = client->send(out);
      if (VSCP_ERROR_SUCCESS != rv) {
        fprintf(stderr, "vscp-send: send failed: %s\n", errorText(rv).c_str());
        failed++;
        if (failed >= 10) {
          fprintf(stderr, "vscp-send: giving up\n");
          g_run = false;
        }
        continue;
      }
      sent++;
      if (echo) {
        printf("%s\n", fmt.format(ex).c_str());
        fflush(stdout);
      }
    }
  }

  if (linger) {
    sleepMs(linger);
  }
  closeClient(client);

  if (conn.verbose) {
    fprintf(stderr, "vscp-send: %llu event(s) sent, %d failed\n", (unsigned long long) sent, failed);
  }
  return failed ? EXIT_ERROR : EXIT_OK;
}
