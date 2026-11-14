// cli-node.h
//
// Node information from the standard registers and the MDF
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifndef VSCPCLI_NODE_H
#define VSCPCLI_NODE_H

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

class CVscpClient;
class CMDF;
class cguid;

namespace vscpcli {

struct NodeInfo {
  int nickname = -1;
  std::string guid;
  std::string mdfUrl; // As stored in the node (registers 0xE0-0xFF)
  std::string firmware;
  uint32_t manufacturerDeviceId    = 0;
  uint32_t manufacturerSubDeviceId = 0;
  uint8_t bootloaderAlgorithm      = 0;
  uint8_t bufferSize               = 0;
  uint8_t pages                    = 0;
  uint32_t familyCode              = 0;
  uint32_t familyType              = 0;
  uint16_t firmwareDeviceCode      = 0;
  uint8_t alarm                    = 0;
  uint8_t errorCounter             = 0;
  uint8_t conformanceMajor         = 0;
  uint8_t conformanceMinor         = 0;
  uint16_t page                    = 0;
  std::string userId;

  nlohmann::json toJson(void) const;
};

/*!
  Read the standard registers (0x80-0xFF) of a node
  @return VSCP_ERROR_SUCCESS or an error code
*/
int
readNodeInfo(CVscpClient &client, uint8_t nickname, cguid &ifGuid, uint32_t timeout, NodeInfo &info);

/*!
  Full download URL for the MDF URL stored in a node (http:// is added when
  there is no scheme, as the registers have room for 32 characters only)
*/
std::string
mdfDownloadUrl(const std::string &stored);

/*!
  Download the MDF to file and parse it
  @return true on success (error is set otherwise)
*/
bool
fetchMdf(const std::string &url, const std::string &file, CMDF &mdf, std::string &error);

/*!
  Name of a bootloader algorithm
*/
std::string
bootloaderName(uint8_t algorithm);

} // namespace vscpcli

#endif
