// vscp-info.cpp
//
// Show information about a VSCP node: GUID, MDF URL, firmware version and
// the other standard registers. Optionally download and show the MDF.
//
//   vscp-info -i udp -N 1
//   vscp-info -i tcpip -H server -N 0x12 --mdf
//   vscp-info -i socketcan -N 7 --mdf-save node7.xml
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <cli-args.h>
#include <cli-connection.h>
#include <cli-node.h>
#include <cli-util.h>

#include <guid.h>
#include <mdf.h>
#include <register.h>
#include <vscp-client-base.h>
#include <vscphelper.h>

using namespace vscpcli;
using json = nlohmann::json;

static std::string
tempFileName(void)
{
#ifdef _WIN32
  const char *dir = getenv("TEMP");
  std::string base = dir ? dir : ".";
  return base + "\\vscp-info-" + std::to_string(msNow()) + ".mdf";
#else
  const char *dir = getenv("TMPDIR");
  std::string base = dir ? dir : "/tmp";
  return base + "/vscp-info-" + std::to_string(msNow()) + ".mdf";
#endif
}

int
main(int argc, char **argv)
{
  ConnectionOptions conn;

  int node = -1;
  std::string ifGuidStr;
  uint32_t timeout = REGISTER_DEFAULT_TIMEOUT;
  bool jsonOut     = false;
  bool showMdf     = false;
  bool guidOnly    = false;
  bool urlOnly     = false;
  std::string mdfSave;
  std::string mdfUrlOverride;

  ArgParser p("vscp-info", "[OPTIONS] -N NODE",
              "Show the GUID, MDF URL, firmware version and the other standard registers\n"
              "of a VSCP node. --mdf also downloads the MDF and shows what it describes.");

  p.group("Node");

  p.option("node", 'N', "NICKNAME", "Nickname of the node (0-255) or its GUID", [&](const std::string &v, std::string &) {
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

  p.option("via", 0, "GUID", "GUID of the server interface the node is on", [&](const std::string &v, std::string &) {
    std::vector<uint8_t> b;
    if (!parseHexString(v, b) || (16 != b.size())) {
      return false;
    }
    ifGuidStr = v;
    return true;
  });

  p.option("timeout", 0, "MS", "Timeout for register reads (default 2000)", [&](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 600000, n) || !n) {
      return false;
    }
    timeout = (uint32_t) n;
    return true;
  });

  p.group("Output");

  p.flag("guid", 0, "Only print the GUID", [&]() { guidOnly = true; });
  p.flag("mdf-url", 0, "Only print the MDF URL", [&]() { urlOnly = true; });
  p.flag("mdf", 'm', "Download the MDF and show module, registers etc.", [&]() { showMdf = true; });
  p.option("mdf-save", 0, "FILE", "Download the MDF to FILE", [&](const std::string &v, std::string &) {
    mdfSave = v;
    return true;
  });
  p.option("mdf-from", 0, "URL", "Use this MDF URL instead of the one in the node", [&](const std::string &v, std::string &) {
    mdfUrlOverride = v;
    return true;
  });
  p.flag("json", 'j', "Output as JSON", [&]() { jsonOut = true; });

  addConnectionOptions(p, conn);

  p.epilog("Examples:\n"
           "  vscp-info -i udp -N 1\n"
           "  vscp-info -i tcpip -H server -N 0x12 --mdf\n"
           "  vscp-info -i socketcan --device can0 -N 7 --guid\n"
           "  vscp-info -i tcpip -N 1 --mdf-save node1.xml -j");

  std::vector<std::string> pos;
  std::string error;
  bool exitNow;
  if (!p.parse(argc, argv, pos, error, exitNow)) {
    fprintf(stderr, "vscp-info: %s\n", error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  if ((node < 0) && (1 == pos.size())) {
    uint64_t n; // Allow the node as an argument too
    if (parseUnsigned(pos[0], 255, n)) {
      node = (int) n;
      pos.clear();
    }
  }
  if (!pos.empty()) {
    fprintf(stderr, "vscp-info: unexpected argument '%s'\n", pos[0].c_str());
    return EXIT_USAGE;
  }
  finishConnectionOptions(conn);

  int rc;
  if (handlePrintConfig(conn, rc)) {
    return rc;
  }

  if (node < 0) {
    fprintf(stderr, "vscp-info: give the node with -N NICKNAME\n");
    return EXIT_USAGE;
  }

  installSignalHandlers();

  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "vscp-info: %s\n", error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  cguid ifGuid;
  if (!ifGuidStr.empty()) {
    ifGuid.getFromString(ifGuidStr);
  }

  NodeInfo info;
  int rv = readNodeInfo(*client, (uint8_t) node, ifGuid, timeout, info);
  closeClient(client);

  if (VSCP_ERROR_SUCCESS != rv) {
    fprintf(stderr, "vscp-info: no answer from node %d: %s\n", node, errorText(rv).c_str());
    return (VSCP_ERROR_TIMEOUT == rv) ? EXIT_TIMEOUT : EXIT_ERROR;
  }

  if (guidOnly) {
    printf("%s\n", info.guid.c_str());
    return EXIT_OK;
  }

  std::string url = mdfDownloadUrl(mdfUrlOverride.empty() ? info.mdfUrl : mdfUrlOverride);
  if (urlOnly) {
    printf("%s\n", url.c_str());
    return EXIT_OK;
  }

  // MDF
  CMDF mdf;
  bool haveMdf = false;
  std::string mdfError;
  if (showMdf || !mdfSave.empty()) {
    if (url.empty()) {
      mdfError = "The node has no MDF URL";
    }
    else {
      std::string file = mdfSave.empty() ? tempFileName() : mdfSave;
      haveMdf          = fetchMdf(url, file, mdf, mdfError);
      if (mdfSave.empty()) {
        remove(file.c_str());
      }
    }
  }

  if (jsonOut) {
    json j = info.toJson();
    j["mdf-download-url"] = url;
    if (haveMdf) {
      json m;
      m["name"]         = mdf.getModuleName();
      m["model"]        = mdf.getModuleModel();
      m["version"]      = mdf.getModuleVersion();
      m["change-date"]  = mdf.getModuleChangeDate();
      m["description"]  = mdf.getModuleDescription();
      m["info-url"]     = mdf.getModuleInfoUrl();
      m["manufacturer"] = mdf.getManufacturerName();
      m["level"]        = mdf.getModuleLevel() + 1;
      m["registers"]    = json::array();
      for (auto preg : *mdf.getRegisterObjList()) {
        m["registers"].push_back({ { "page", preg->getPage() },
                                   { "offset", preg->getOffset() },
                                   { "name", preg->getName() },
                                   { "access", preg->getAccessStr() } });
      }
      j["mdf"] = m;
      if (!mdfSave.empty()) {
        j["mdf-file"] = mdfSave;
      }
    }
    else if (!mdfError.empty()) {
      j["mdf-error"] = mdfError;
    }
    printf("%s\n", j.dump(2).c_str());
  }
  else {
    printf("Nickname           %d (0x%02X)\n", info.nickname, info.nickname);
    printf("GUID               %s\n", info.guid.c_str());
    printf("MDF URL            %s\n", url.empty() ? "(none)" : url.c_str());
    printf("Firmware           %s\n", info.firmware.c_str());
    printf("Manufacturer id    0x%08X / sub 0x%08X\n", info.manufacturerDeviceId, info.manufacturerSubDeviceId);
    printf("Bootloader         %s (0x%02X)\n", bootloaderName(info.bootloaderAlgorithm).c_str(),
           info.bootloaderAlgorithm);
    printf("Buffer size        %u\n", info.bufferSize);
    printf("Register pages     %u (selected page %u)\n", info.pages, info.page);
    printf("Family code/type   0x%08X / 0x%08X\n", info.familyCode, info.familyType);
    printf("Firmware dev. code 0x%04X\n", info.firmwareDeviceCode);
    printf("Alarm              0x%02X\n", info.alarm);
    printf("Error counter      %u\n", info.errorCounter);
    printf("Conformance        %u.%u\n", info.conformanceMajor, info.conformanceMinor);
    printf("User id            %s\n", info.userId.c_str());

    if (haveMdf) {
      printf("\nMDF\n");
      printf("  Module           %s\n", mdf.getModuleName().c_str());
      printf("  Model            %s\n", mdf.getModuleModel().c_str());
      printf("  Version          %s (%s)\n", mdf.getModuleVersion().c_str(), mdf.getModuleChangeDate().c_str());
      printf("  Manufacturer     %s\n", mdf.getManufacturerName().c_str());
      printf("  Description      %s\n", trim(mdf.getModuleDescription()).c_str());
      printf("  Info URL         %s\n", mdf.getModuleInfoUrl().c_str());
      auto *regs = mdf.getRegisterObjList();
      printf("  Registers        %zu\n", regs->size());
      for (auto preg : *regs) {
        printf("    %04X:%02X  %-3s %s\n", preg->getPage(), preg->getOffset(), preg->getAccessStr().c_str(),
               preg->getName().c_str());
      }
      if (!mdfSave.empty()) {
        printf("  Saved to         %s\n", mdfSave.c_str());
      }
    }
  }

  if (!mdfError.empty()) {
    fprintf(stderr, "vscp-info: %s\n", mdfError.c_str());
    return EXIT_ERROR;
  }
  return EXIT_OK;
}
