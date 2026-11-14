// cli-connection.h
//
// Connection options shared by all VSCP command line tools and the code that
// turns them into a connected VSCP client for any of the VSCP transports:
//
//   tcpip      VSCP tcp/ip link protocol (text or binary, TLS, AES + HMAC)
//   udp        VSCP over UDP (AES + HMAC)
//   multicast  VSCP multicast (AES + HMAC)
//   ws1, ws2   VSCP websocket protocols (TLS, AES authentication)
//   mqtt       VSCP over MQTT (TLS)
//   socketcan  VSCP over SocketCAN (Linux)
//   canal      CANAL (VSCP Level I) driver
//   level2     VSCP Level II driver
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifndef VSCPCLI_CONNECTION_H
#define VSCPCLI_CONNECTION_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "cli-args.h"

class CVscpClient;

namespace vscpcli {

struct ConnectionOptions {
  std::string iface;      // Interface type (see above)
  std::string configFile; // JSON file with client settings
  std::vector<std::pair<std::string, nlohmann::json>> sets; // --set key=value

  std::string host;
  uint16_t port = 0;
  std::string user;
  std::string password;
  bool passwordSet = false;

  std::string device;       // SocketCAN device
  std::string driver;       // CANAL / Level II driver library
  std::string driverConfig; // CANAL configuration string / Level II configuration file
  uint32_t flags = 0;
  bool flagsSet  = false;
  std::string guid;         // Interface GUID (socketcan, level2)
  std::string netInterface; // Network interface (udp, multicast)

  // Security
  bool tls = false;
  std::string caFile;
  std::string caPath;
  std::string certFile;
  std::string keyFile;
  bool insecure = false;
  std::string encryption; // none, aes128, aes192, aes256
  std::string key;        // AES key (hex)
  std::string hmacKey;    // HMAC-SHA256 key (hex)
  bool requireEncryption = false;
  bool binary            = false;

  // MQTT
  std::vector<std::string> subscribe;
  std::vector<std::string> publish;
  std::string mqttFormat;
  int qos = -1;

  uint32_t connectTimeout  = 5000;
  uint32_t responseTimeout = 3000;

  int verbose      = 0;
  bool printConfig = false;

  bool configError = false; // Set by openClient when the settings are invalid
};

/*!
  Add the connection and security options to a parser
*/
void
addConnectionOptions(ArgParser &parser, ConnectionOptions &opts);

/*!
  Read password and keys from the environment (VSCP_PASSWORD, VSCP_KEY,
  VSCP_HMAC_KEY) when they are not given on the command line, and set up
  logging. Call after parsing.
*/
void
finishConnectionOptions(ConnectionOptions &opts);

/*!
  Normalized interface name ("tcp" -> "tcpip" etc.), empty if unknown
*/
std::string
interfaceName(const std::string &name);

/*!
  Build the JSON configuration handed to the client.
  @return false on error (error is set)
*/
bool
buildClientConfig(ConnectionOptions &opts, nlohmann::json &cfg, std::string &error);

/*!
  Create, configure and connect a client. Waits for clients that connect in
  the background (MQTT) until the connect timeout.
  @return The connected client or nullptr (error is set)
*/
std::unique_ptr<CVscpClient>
openClient(ConnectionOptions &opts, std::string &error);

/*!
  Disconnect (errors are ignored)
*/
void
closeClient(std::unique_ptr<CVscpClient> &client);

/*!
  Handle --print-config: prints the client configuration (keys and the
  password are masked). Returns true if the program should exit.
*/
bool
handlePrintConfig(ConnectionOptions &opts, int &exitCode);

} // namespace vscpcli

#endif
