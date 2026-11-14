// cli-node.cpp
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include "cli-node.h"
#include "cli-util.h"

#include <cstdio>

#include <guid.h>
#include <mdf.h>
#include <register.h>
#include <vscp-client-base.h>
#include <vscp.h>
#include <vscphelper.h>

using json = nlohmann::json;

namespace vscpcli {

json
NodeInfo::toJson(void) const
{
  char buf[16];
  json j;
  j["nickname"] = nickname;
  j["guid"]     = guid;
  j["mdf-url"]  = mdfUrl;
  j["firmware"] = firmware;
  snprintf(buf, sizeof(buf), "0x%08X", manufacturerDeviceId);
  j["manufacturer-device-id"] = buf;
  snprintf(buf, sizeof(buf), "0x%08X", manufacturerSubDeviceId);
  j["manufacturer-sub-device-id"] = buf;
  j["bootloader-algorithm"]       = bootloaderAlgorithm;
  j["bootloader"]                 = bootloaderName(bootloaderAlgorithm);
  j["buffer-size"]                = bufferSize;
  j["register-pages"]             = pages;
  j["family-code"]                = familyCode;
  j["family-type"]                = familyType;
  j["firmware-device-code"]       = firmwareDeviceCode;
  j["alarm"]                      = alarm;
  j["error-counter"]              = errorCounter;
  j["conformance"]                = std::to_string(conformanceMajor) + "." + std::to_string(conformanceMinor);
  j["page"]                       = page;
  j["user-id"]                    = userId;
  return j;
}

int
readNodeInfo(CVscpClient &client, uint8_t nickname, cguid &ifGuid, uint32_t timeout, NodeInfo &info)
{
  cguid guidNode;
  guidNode.setLSB(nickname);

  CStandardRegisters regs;
  int rv = regs.init(client, guidNode, ifGuid, nullptr, timeout);
  if (VSCP_ERROR_SUCCESS != rv) {
    return rv;
  }

  info.nickname = nickname;
  info.guid     = regs.getGUIDStr();
  info.mdfUrl   = trim(regs.getMDF());
  // Only printable characters (unused URL bytes may hold anything)
  std::string clean;
  for (char c : info.mdfUrl) {
    if ((c > ' ') && (c < 0x7f)) {
      clean += c;
    }
  }
  info.mdfUrl                  = clean;
  info.firmware                = regs.getFirmwareVersionString();
  info.manufacturerDeviceId    = regs.getManufacturerDeviceID();
  info.manufacturerSubDeviceId = regs.getManufacturerSubDeviceID();
  info.bootloaderAlgorithm     = regs.getBootloaderAlgorithm();
  info.bufferSize              = regs.getBufferSize();
  info.pages                   = regs.getNumberOfRegisterPages();
  info.familyCode              = regs.getStandardDeviceFamilyCode();
  info.familyType              = regs.getStandardDeviceFamilyType();
  info.firmwareDeviceCode      = regs.getFirmwareDeviceCode();
  info.alarm                   = regs.getAlarm();
  info.errorCounter            = regs.getErrorCounter();
  info.conformanceMajor        = regs.getConformanceVersionMajor();
  info.conformanceMinor        = regs.getConformanceVersionMinor();
  info.page                    = regs.getRegisterPage();

  char buf[8];
  info.userId.clear();
  for (uint8_t i = 0; i < 5; i++) {
    snprintf(buf, sizeof(buf), "%s%02X", i ? ":" : "", regs.getUserId(i));
    info.userId += buf;
  }
  return VSCP_ERROR_SUCCESS;
}

std::string
mdfDownloadUrl(const std::string &stored)
{
  std::string url = trim(stored);
  if (url.empty()) {
    return url;
  }
  std::string l = lower(url);
  if ((0 == l.compare(0, 7, "http://")) || (0 == l.compare(0, 8, "https://"))) {
    return url;
  }
  return "http://" + url;
}

bool
fetchMdf(const std::string &url, const std::string &file, CMDF &mdf, std::string &error)
{
  CURLcode cc = mdf.downLoadMDF(url, file);
  if (CURLE_OK != cc) {
    error = "Unable to download the MDF from " + url + " (" + curl_easy_strerror(cc) + ")";
    return false;
  }
  int rv = mdf.parseMDF(file);
  if (VSCP_ERROR_SUCCESS != rv) {
    error = "Unable to parse the MDF from " + url + " (" + errorText(rv) + ")";
    return false;
  }
  return true;
}

std::string
bootloaderName(uint8_t a)
{
  switch (a) {
    case VSCP_BOOTLOADER_VSCP:
      return "VSCP";
    case VSCP_BOOTLOADER_PIC1:
      return "PIC1";
    case VSCP_BOOTLOADER_AVR1:
      return "AVR1";
    case VSCP_BOOTLOADER_LPC1:
      return "NXP/LPC1";
    case VSCP_BOOTLOADER_ST:
      return "ST";
    case VSCP_BOOTLOADER_FREESCALE:
      return "Freescale";
    case VSCP_BOOTLOADER_ESP:
      return "Espressif";
    case 0xff:
      return "none";
    default:
      if (a >= 0xf0) {
        return "user defined " + std::to_string(a - 0xf0);
      }
      return "unknown";
  }
}

} // namespace vscpcli
