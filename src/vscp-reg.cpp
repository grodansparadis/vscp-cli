// vscp-reg.cpp
//
// Read and write registers of VSCP nodes (VSCP register protocol, Level I
// register events, also through a Level II interface).
//
//   vscp-reg -i udp -N 1 read 0x80 16
//   vscp-reg -i tcpip -N 0x12 --page 1 write 0x10 1 2 3
//   vscp-reg -i socketcan -N 7 dump
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cli-args.h>
#include <cli-connection.h>
#include <cli-util.h>

#include <guid.h>
#include <register.h>
#include <vscp-client-base.h>
#include <vscphelper.h>

using namespace vscpcli;
using json = nlohmann::json;

// Print registers as a hex dump, 16 per line
static void
printDump(const std::map<uint8_t, uint8_t> &regs, uint16_t page, bool hasPage)
{
  printf("%-10s", "");
  for (int i = 0; i < 16; i++) {
    printf(" %X ", i);
  }
  printf("\n");

  int lastRow = -1;
  std::string ascii;
  auto endRow = [&]() {
    if (lastRow >= 0) {
      printf("  %s\n", ascii.c_str());
    }
  };

  for (const auto &r : regs) {
    int row = r.first & 0xf0;
    if (row != lastRow) {
      endRow();
      ascii.clear();
      if (hasPage && (row < 0x80)) {
        printf("%04X:%02X  ", page, row);
      }
      else {
        printf("     %02X  ", row);
      }
      // Fill out missing registers at the start of a row
      for (int i = row; i < r.first; i++) {
        printf(" --");
        ascii += ' ';
      }
      lastRow = row;
    }
    printf(" %02X", r.second);
    ascii += isprint(r.second) ? (char) r.second : '.';
  }
  endRow();
}

int
main(int argc, char **argv)
{
  ConnectionOptions conn;

  int node          = -1;
  uint16_t page     = 0;
  bool pageSet      = false;
  std::string ifGuidStr;
  uint32_t timeout  = REGISTER_DEFAULT_TIMEOUT;
  bool jsonOut      = false;
  bool quiet        = false;

  ArgParser p("vscp-reg",
              "[OPTIONS] -N NODE read REG [COUNT]\n"
              "       vscp-reg [OPTIONS] -N NODE write REG VALUE [VALUE ...]\n"
              "       vscp-reg [OPTIONS] -N NODE dump [user|standard|all]",
              "Read and write registers of a VSCP node with the VSCP register protocol.\n"
              "Registers 0x00-0x7F are the user registers of the page (--page), 0x80-0xFF\n"
              "are the standard registers. Values are decimal or hex (0x..).");

  p.group("Node");

  p.option("node", 'N', "NICKNAME", "Nickname of the node (0-255), or its GUID (the last\nbyte is the nickname)",
           [&](const std::string &v, std::string &) {
             uint64_t n;
             if (parseUnsigned(v, 255, n)) {
               node = (int) n;
               return true;
             }
             std::vector<uint8_t> b;
             if (parseHexString(v, b) && (16 == b.size())) {
               node = b[15];
               return true;
             }
             return false;
           });

  p.option("page", 0, "N", "Register page for 0x00-0x7F (default 0)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 0xffff, n)) {
      return false;
    }
    page    = (uint16_t) n;
    pageSet = true;
    return true;
  });

  p.option("via", 0, "GUID",
           "GUID of the server interface the node is on. Register\n"
           "events then go as CLASS2.LEVEL1.PROTOCOL to it.",
           [&](const std::string &v, std::string &) {
             std::vector<uint8_t> b;
             if (!parseHexString(v, b) || (16 != b.size())) {
               return false;
             }
             ifGuidStr = v;
             return true;
           });

  p.option("timeout", 0, "MS", "Timeout for each register operation (default 2000)",
           [&](const std::string &v, std::string &) {
             uint64_t n;
             if (!parseUnsigned(v, 600000, n) || !n) {
               return false;
             }
             timeout = (uint32_t) n;
             return true;
           });

  p.group("Output");

  p.flag("json", 'j', "Output as JSON", [&]() { jsonOut = true; });
  p.flag("quiet", 'q', "No output from write (exit status only)", [&]() { quiet = true; });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-reg -i udp -N 1 read 0x80 16          first 16 standard registers\n"
           "  vscp-reg -i tcpip -N 1 --page 2 read 0 128  all user registers on page 2\n"
           "  vscp-reg -i tcpip -N 1 write 0x10 0x55      write one register\n"
           "  vscp-reg -i socketcan -N 7 write 0 1 2 3    write registers 0, 1 and 2\n"
           "  vscp-reg -i tcpip -N 1 dump standard -j\n"
           "\n"
           "Exit status: 0 ok, 1 error, 2 usage, 3 connection, 4 no answer from the node.");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-reg: %s\n", error.c_str());
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

  if (node < 0) {
    fprintf(stderr, "vscp-reg: give the node with -N NICKNAME\n");
    return EXIT_USAGE;
  }
  if (pos.empty()) {
    fprintf(stderr, "vscp-reg: give a command: read, write or dump (see --help)\n");
    return EXIT_USAGE;
  }

  std::string cmd = lower(pos[0]);
  uint64_t reg    = 0;
  uint64_t count  = 1;
  std::vector<uint8_t> values;
  std::string dumpWhat = "all";

  if ("read" == cmd) {
    if ((pos.size() < 2) || (pos.size() > 3) || !parseUnsigned(pos[1], 255, reg) ||
        ((3 == pos.size()) && (!parseUnsigned(pos[2], 256, count) || !count))) {
      fprintf(stderr, "vscp-reg: use read REG [COUNT], REG 0-255, COUNT 1-256\n");
      return EXIT_USAGE;
    }
    if (reg + count > 256) {
      fprintf(stderr, "vscp-reg: REG + COUNT is past register 0xFF\n");
      return EXIT_USAGE;
    }
  }
  else if ("write" == cmd) {
    if ((pos.size() < 3) || !parseUnsigned(pos[1], 255, reg)) {
      fprintf(stderr, "vscp-reg: use write REG VALUE [VALUE ...]\n");
      return EXIT_USAGE;
    }
    for (size_t i = 2; i < pos.size(); i++) {
      if (!parseByteList(pos[i], values, error)) {
        fprintf(stderr, "vscp-reg: %s\n", error.c_str());
        return EXIT_USAGE;
      }
    }
    if (reg + values.size() > 256) {
      fprintf(stderr, "vscp-reg: the values go past register 0xFF\n");
      return EXIT_USAGE;
    }
  }
  else if ("dump" == cmd) {
    if (pos.size() > 1) {
      dumpWhat = lower(pos[1]);
    }
    if ((pos.size() > 2) || (("user" != dumpWhat) && ("standard" != dumpWhat) && ("all" != dumpWhat))) {
      fprintf(stderr, "vscp-reg: use dump [user|standard|all]\n");
      return EXIT_USAGE;
    }
    if ("user" == dumpWhat) {
      reg   = 0;
      count = 128;
    }
    else if ("standard" == dumpWhat) {
      reg   = 0x80;
      count = 128;
    }
    else {
      reg   = 0;
      count = 256;
    }
  }
  else {
    fprintf(stderr, "vscp-reg: unknown command '%s' (read, write or dump)\n", pos[0].c_str());
    return EXIT_USAGE;
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-reg: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  cguid guidNode;
  guidNode.setLSB((uint8_t) node);
  cguid guidIf;
  if (!ifGuidStr.empty()) {
    guidIf.getFromString(ifGuidStr);
  }

  int result = EXIT_OK;

  if (("read" == cmd) || ("dump" == cmd)) {
    std::map<uint8_t, uint8_t> regs;
    int rv = VSCP_ERROR_SUCCESS;

    // The library reads at most 256 - offset registers in one block, and
    // standard registers do not depend on the page.
    rv = vscp_readLevel1RegisterBlock(*client, guidNode, guidIf, page, (uint8_t) reg, (uint8_t) (count & 0xff), regs,
                                      nullptr, timeout);

    if (VSCP_ERROR_SUCCESS != rv) {
      fprintf(stderr, "vscp-reg: reading node %d failed: %s\n", node, errorText(rv).c_str());
      result = (VSCP_ERROR_TIMEOUT == rv) ? EXIT_TIMEOUT : EXIT_ERROR;
    }
    else if (jsonOut) {
      json j;
      j["node"]      = node;
      j["page"]      = page;
      j["registers"] = json::array();
      for (const auto &r : regs) {
        j["registers"].push_back({ { "register", r.first }, { "value", r.second } });
      }
      printf("%s\n", j.dump(2).c_str());
    }
    else if (("read" == cmd) && (count <= 16) && (regs.size() <= 16)) {
      for (const auto &r : regs) {
        if (r.first < 0x80) {
          printf("%04X:%02X  0x%02X  %3u\n", page, r.first, r.second, r.second);
        }
        else {
          printf("     %02X  0x%02X  %3u\n", r.first, r.second, r.second);
        }
      }
    }
    else {
      printDump(regs, page, pageSet || (reg < 0x80));
    }
  }
  else { // write
    int failed = 0;
    json j     = json::array();
    for (size_t i = 0; (i < values.size()) && g_run; i++) {
      uint8_t r = (uint8_t) (reg + i);
      int rv    = vscp_writeLevel1Register(*client, guidNode, guidIf, page, r, values[i], timeout);
      if (VSCP_ERROR_SUCCESS == rv) {
        if (jsonOut) {
          j.push_back({ { "register", r }, { "value", values[i] }, { "ok", true } });
        }
        else if (!quiet) {
          printf("%02X <- 0x%02X ok\n", r, values[i]);
        }
        continue;
      }

      failed++;
      // A write is confirmed with the new value. Read the register to tell a
      // read only register from a node that does not answer.
      uint8_t now = 0;
      int rv2     = vscp_readLevel1Register(*client, guidNode, guidIf, page, r, now, timeout);
      if (VSCP_ERROR_SUCCESS == rv2) {
        fprintf(stderr, "vscp-reg: register 0x%02X is 0x%02X after writing 0x%02X (read only or limited)\n", r, now,
                values[i]);
        if (jsonOut) {
          j.push_back({ { "register", r }, { "value", values[i] }, { "ok", false }, { "actual", now } });
        }
        if (EXIT_OK == result) {
          result = EXIT_ERROR;
        }
      }
      else {
        fprintf(stderr, "vscp-reg: writing register 0x%02X of node %d failed: %s\n", r, node, errorText(rv).c_str());
        if (jsonOut) {
          j.push_back({ { "register", r }, { "value", values[i] }, { "ok", false } });
        }
        result = (VSCP_ERROR_TIMEOUT == rv) ? EXIT_TIMEOUT : EXIT_ERROR;
        break; // The node does not answer, don't wait for the rest
      }
    }
    if (jsonOut) {
      printf("%s\n", j.dump(2).c_str());
    }
  }

  closeClient(client);
  return result;
}
