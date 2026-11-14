// vscp-scan.cpp
//
// Find the VSCP nodes on a segment.
//
//   Fast scan (default): one "who is there" event, all nodes answer with
//   their GUID and MDF URL.
//   Slow scan (--slow): reads a register of every nickname in a range. Finds
//   nodes that do not implement "who is there".
//
//   vscp-scan -i socketcan --device can0
//   vscp-scan -i tcpip -H server --slow --from 1 --to 32 --info
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cli-args.h>
#include <cli-connection.h>
#include <cli-node.h>
#include <cli-util.h>

#include <guid.h>
#include <register.h>
#include <vscp-client-base.h>
#include <vscphelper.h>

using namespace vscpcli;
using json = nlohmann::json;

struct Found {
  uint8_t nickname = 0;
  std::string eventGuid; // GUID of the answering events
  std::string guid;      // Node GUID from the answer / registers
  std::string mdfUrl;
  bool complete = false;
  // Level I who is there answer: 7 frames x 7 bytes (GUID + MDF URL)
  uint8_t buf[49]  = { 0 };
  uint8_t gotFrames = 0; // Bit per frame index
  NodeInfo info;
  bool haveInfo = false;
};

static std::string
guidString(const uint8_t *p)
{
  cguid g;
  g.getFromArray(p);
  return g.getAsString();
}

static std::string
urlString(const uint8_t *p, size_t n)
{
  std::string s;
  for (size_t i = 0; i < n; i++) {
    if (!p[i]) {
      break;
    }
    if ((p[i] > ' ') && (p[i] < 0x7f)) {
      s += (char) p[i];
    }
  }
  return s;
}

int
main(int argc, char **argv)
{
  ConnectionOptions conn;

  bool slow        = false;
  uint64_t first   = 0;
  uint64_t last    = 254;
  uint32_t timeout = 0;
  bool timeoutSet  = false;
  uint32_t delay   = 5;
  bool info        = false;
  bool jsonOut     = false;
  std::string ifGuidStr;

  ArgParser p("vscp-scan", "[OPTIONS]",
              "Find the VSCP nodes on a segment. The default fast scan sends one \"who is\n"
              "there\" event that all nodes answer with their GUID and MDF URL. --slow reads\n"
              "a register of every nickname in a range instead.");

  p.group("Scan");

  p.flag("slow", 's', "Register read scan of nicknames --from..--to", [&]() { slow = true; });

  p.option("from", 0, "N", "First nickname for --slow (default 0)", [&](const std::string &v, std::string &) {
    return parseUnsigned(v, 255, first);
  });

  p.option("to", 0, "N", "Last nickname for --slow (default 254)", [&](const std::string &v, std::string &) {
    return parseUnsigned(v, 255, last);
  });

  p.option("timeout", 0, "MS", "How long to wait for answers (default 2000 fast,\n3000 slow)",
           [&](const std::string &v, std::string &) {
             uint64_t n;
             if (!parseUnsigned(v, 600000, n) || !n) {
               return false;
             }
             timeout    = (uint32_t) n;
             timeoutSet = true;
             return true;
           });

  p.option("delay", 0, "MS", "--slow: delay between the read requests (default 5)",
           [&](const std::string &v, std::string &) {
             uint64_t n;
             if (!parseUnsigned(v, 10000, n)) {
               return false;
             }
             delay = (uint32_t) n;
             return true;
           });

  p.option("via", 0, "GUID", "GUID of the server interface to scan behind", [&](const std::string &v, std::string &) {
    std::vector<uint8_t> b;
    if (!parseHexString(v, b) || (16 != b.size())) {
      return false;
    }
    ifGuidStr = v;
    return true;
  });

  p.group("Output");

  p.flag("info", 0, "Read the standard registers of each node found\n(firmware, manufacturer id, ...)",
         [&]() { info = true; });
  p.flag("json", 'j', "Output as JSON", [&]() { jsonOut = true; });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-scan -i socketcan --device can0\n"
           "  vscp-scan -i udp --info\n"
           "  vscp-scan -i tcpip -H server --slow --from 1 --to 32 -j\n"
           "\n"
           "Exit status: 0 nodes found, 4 no node answered.");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-scan: %s\n", error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  if (!pos.empty()) {
    fprintf(stderr, "vscp-scan: unexpected argument '%s'\n", pos[0].c_str());
    return EXIT_USAGE;
  }
  if (first > last) {
    fprintf(stderr, "vscp-scan: --from must not be larger than --to\n");
    return EXIT_USAGE;
  }
  finishConnectionOptions(conn);

  int rc;
  if (handlePrintConfig(conn, rc)) {
    return rc;
  }

  if (!timeoutSet) {
    timeout = slow ? 3000 : 2000;
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-scan: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  cguid ifGuid;
  if (!ifGuidStr.empty()) {
    ifGuid.getFromString(ifGuidStr);
  }
  const bool via      = !ifGuid.isNULL();
  const uint8_t off   = via ? 16 : 0;
  const uint16_t pcls = via ? VSCP_CLASS2_LEVEL1_PROTOCOL : VSCP_CLASS1_PROTOCOL;

  std::map<uint8_t, Found> found;

  client->clear();

  // Send the request(s)
  vscpEventEx ex;
  memset(&ex, 0, sizeof(ex));
  ex.vscp_class = pcls;
  if (via) {
    memcpy(ex.data, ifGuid.getGUID(), 16);
  }

  int rv = VSCP_ERROR_SUCCESS;
  if (!slow) {
    ex.vscp_type     = VSCP_TYPE_PROTOCOL_WHO_IS_THERE;
    ex.sizeData      = 1 + off;
    ex.data[off]     = 0xff; // All nodes
    rv               = client->send(ex);
  }
  else {
    ex.vscp_type = VSCP_TYPE_PROTOCOL_READ_REGISTER;
    ex.sizeData  = 2 + off;
    for (uint64_t n = first; (n <= last) && g_run; n++) {
      ex.data[off]     = (uint8_t) n;
      ex.data[off + 1] = 0xd0; // First GUID byte
      if (VSCP_ERROR_SUCCESS != (rv = client->send(ex))) {
        break;
      }
      if (delay) {
        sleepMs(delay);
      }
    }
  }
  if (VSCP_ERROR_SUCCESS != rv) {
    fprintf(stderr, "vscp-scan: sending failed: %s\n", errorText(rv).c_str());
    closeClient(client);
    return EXIT_ERROR;
  }

  // Collect answers
  uint64_t start = msNow();
  while (g_run && ((msNow() - start) < timeout)) {
    vscpEventEx e;
    memset(&e, 0, sizeof(e));
    rv = client->receiveBlocking(e, 50);
    if (VSCP_ERROR_SUCCESS != rv) {
      continue;
    }

    const bool l1   = (VSCP_CLASS1_PROTOCOL == e.vscp_class);
    const bool l1o2 = (VSCP_CLASS2_LEVEL1_PROTOCOL == e.vscp_class);
    if (!l1 && !l1o2 && (VSCP_CLASS2_PROTOCOL != e.vscp_class)) {
      continue;
    }
    const uint8_t *d = e.data + (l1o2 ? 16 : 0);
    int size         = e.sizeData - (l1o2 ? 16 : 0);
    uint8_t nick     = e.GUID[15];

    if (!slow && (l1 || l1o2) && (VSCP_TYPE_PROTOCOL_WHO_IS_THERE_RESPONSE == e.vscp_type) && (size >= 1)) {
      Found &f    = found[nick];
      f.nickname  = nick;
      f.eventGuid = guidString(e.GUID);
      uint8_t idx = d[0];
      if (idx < 7) {
        for (int i = 1; (i < size) && (i < 8); i++) {
          f.buf[idx * 7 + i - 1] = d[i];
        }
        f.gotFrames |= (uint8_t) (1 << idx);
      }
      if (0x7f == f.gotFrames) {
        // The specification lists the GUID bytes as GUID15..GUID0 (MSB first)
        // but the VSCP firmware (vscp-firmware.c, vscp-firmware-level2.c)
        // sends guid[15] (the LSB) first. Decode the way the nodes in use do
        // it. With --info the GUID from the registers (0xD0 = MSB) is used.
        uint8_t guid[16];
        for (int i = 0; i < 16; i++) {
          guid[i] = f.buf[15 - i];
        }
        f.complete = true;
        f.guid     = guidString(guid);
        f.mdfUrl   = urlString(f.buf + 16, 32);
      }
    }
    else if (!slow && (VSCP_CLASS2_PROTOCOL == e.vscp_class) &&
             (VSCP2_TYPE_PROTOCOL_WHO_IS_THERE_RESPONSE == e.vscp_type) && (size >= 16)) {
      // Level II answer: GUID, then the MDF URL
      Found &f    = found[nick];
      f.nickname  = nick;
      f.eventGuid = guidString(e.GUID);
      f.guid      = guidString(d);
      f.mdfUrl    = urlString(d + 16, (size_t) std::max(0, std::min(size - 16, 32)));
      f.complete  = true;
    }
    else if (slow && (l1 || l1o2) && (VSCP_TYPE_PROTOCOL_RW_RESPONSE == e.vscp_type) && (size >= 2) &&
             (0xd0 == d[0]) && (nick >= first) && (nick <= last)) {
      Found &f    = found[nick];
      f.nickname  = nick;
      f.eventGuid = guidString(e.GUID);
      if (slow && (found.size() >= (last - first + 1))) {
        break;
      }
    }
  }

  // Details for each node
  for (auto &kv : found) {
    if (!g_run) {
      break;
    }
    Found &f = kv.second;
    if (info || f.guid.empty()) {
      if (VSCP_ERROR_SUCCESS == readNodeInfo(*client, f.nickname, ifGuid, REGISTER_DEFAULT_TIMEOUT, f.info)) {
        f.haveInfo = true;
        f.guid     = f.info.guid; // The registers are unambiguous
        if (f.mdfUrl.empty()) {
          f.mdfUrl = f.info.mdfUrl;
        }
      }
    }
  }

  closeClient(client);

  if (jsonOut) {
    json j = json::array();
    for (auto &kv : found) {
      Found &f = kv.second;
      json n;
      n["nickname"] = f.nickname;
      n["guid"]     = f.guid;
      n["mdf-url"]  = mdfDownloadUrl(f.mdfUrl);
      if (f.haveInfo && info) {
        n["info"] = f.info.toJson();
      }
      j.push_back(n);
    }
    printf("%s\n", j.dump(2).c_str());
  }
  else if (found.empty()) {
    fprintf(stderr, "vscp-scan: no node answered\n");
  }
  else {
    printf("%-5s %-47s  %s\n", "Node", "GUID", "MDF URL");
    for (auto &kv : found) {
      Found &f = kv.second;
      printf("%3u   %-47s  %s\n", f.nickname, f.guid.empty() ? "?" : f.guid.c_str(), mdfDownloadUrl(f.mdfUrl).c_str());
      if (info && f.haveInfo) {
        printf("      firmware %s, manufacturer id 0x%08X/0x%08X, bootloader %s, %u page(s)\n",
               f.info.firmware.c_str(), f.info.manufacturerDeviceId, f.info.manufacturerSubDeviceId,
               bootloaderName(f.info.bootloaderAlgorithm).c_str(), f.info.pages);
      }
    }
    printf("%zu node(s) found\n", found.size());
  }

  return found.empty() ? EXIT_TIMEOUT : EXIT_OK;
}
