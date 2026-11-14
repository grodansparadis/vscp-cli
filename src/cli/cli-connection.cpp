// cli-connection.cpp
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#include "cli-connection.h"
#include "cli-util.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#include <guid.h>
#include <spdlog/spdlog.h>
#include <vscp.h>
#include <vscphelper.h>

#include <vscp-client-base.h>
#include <vscp-client-canal.h>
#include <vscp-client-level2.h>
#include <vscp-client-mqtt.h>
#include <vscp-client-multicast.h>
#include <vscp-client-tcp.h>
#include <vscp-client-udp.h>
#include <vscp-client-ws1.h>
#include <vscp-client-ws2.h>
#ifdef VSCPCLI_HAVE_SOCKETCAN
#include <vscp-client-socketcan.h>
#endif

using json = nlohmann::json;

namespace vscpcli {

///////////////////////////////////////////////////////////////////////////////
// addConnectionOptions
//

void
addConnectionOptions(ArgParser &p, ConnectionOptions &o)
{
  p.group("Connection");

  p.option("interface", 'i', "TYPE",
           "Interface: tcpip, udp, multicast, ws1, ws2, mqtt,\n"
           "socketcan (Linux), canal or level2",
           [&o](const std::string &v, std::string &err) {
             o.iface = interfaceName(v);
             if (o.iface.empty()) {
               err = "unknown interface";
               return false;
             }
             return true;
           });

  p.option("config", 'c', "FILE",
           "JSON file with interface settings (the client's own\n"
           "keys). Command line options override it.",
           [&o](const std::string &v, std::string &) {
             o.configFile = v;
             return true;
           });

  p.option("set", 0, "KEY=VALUE",
           "Set any client setting (JSON value or text),\n"
           "e.g. --set response-timeout=5000. Repeatable.",
           [&o](const std::string &v, std::string &err) {
             size_t eq = v.find('=');
             if (std::string::npos == eq || 0 == eq) {
               err = "use KEY=VALUE";
               return false;
             }
             std::string val = v.substr(eq + 1);
             json jv;
             try {
               jv = json::parse(val);
             }
             catch (...) {
               jv = val; // Plain text
             }
             o.sets.emplace_back(v.substr(0, eq), jv);
             return true;
           });

  p.option("host", 'H', "HOST", "Host name or address of the server/broker/group",
           [&o](const std::string &v, std::string &) {
             o.host = v;
             return true;
           });

  p.option("port", 'P', "PORT", "Port", [&o](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 65535, n) || !n) {
      return false;
    }
    o.port = (uint16_t) n;
    return true;
  });

  p.option("user", 'u', "USER", "User name", [&o](const std::string &v, std::string &) {
    o.user = v;
    return true;
  });

  p.option("password", 0, "PASSWORD", "Password (or set VSCP_PASSWORD)", [&o](const std::string &v, std::string &) {
    o.password    = v;
    o.passwordSet = true;
    return true;
  });

  p.option("net-interface", 0, "NAME",
           "udp/multicast: network interface to use (e.g. eth0,\n"
           "lo). Default: the one the route points to.",
           [&o](const std::string &v, std::string &) {
             o.netInterface = v;
             return true;
           });

  p.option("device", 0, "DEV", "SocketCAN device (default vcan0)", [&o](const std::string &v, std::string &) {
    o.device = v;
    return true;
  });

  p.option("driver", 0, "PATH", "CANAL or Level II driver library", [&o](const std::string &v, std::string &) {
    o.driver = v;
    return true;
  });

  p.option("driver-config", 0, "CFG",
           "CANAL configuration string, or Level II driver\n"
           "configuration file",
           [&o](const std::string &v, std::string &) {
             o.driverConfig = v;
             return true;
           });

  p.option("flags", 0, "N", "CANAL / SocketCAN flags", [&o](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 0xffffffff, n)) {
      return false;
    }
    o.flags    = (uint32_t) n;
    o.flagsSet = true;
    return true;
  });

  p.option("if-guid", 0, "GUID", "Interface GUID (socketcan, level2)", [&o](const std::string &v, std::string &) {
    o.guid = v;
    return true;
  });

  p.option("connect-timeout", 0, "MS", "Connect timeout in ms (default 5000)", [&o](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 3600000, n)) {
      return false;
    }
    o.connectTimeout = (uint32_t) n;
    return true;
  });

  p.option("response-timeout", 0, "MS", "Server response timeout in ms (default 3000)",
           [&o](const std::string &v, std::string &) {
             uint64_t n;
             if (!parseUnsigned(v, 3600000, n)) {
               return false;
             }
             o.responseTimeout = (uint32_t) n;
             return true;
           });

  p.group("Security");

  p.flag("tls", 0, "Use TLS (tcpip, ws1, ws2, mqtt)", [&o]() { o.tls = true; });

  p.option("ca-file", 0, "FILE", "CA certificate(s) to verify the server with",
           [&o](const std::string &v, std::string &) {
             o.caFile = v;
             return true;
           });

  p.option("ca-path", 0, "DIR", "Directory with CA certificates", [&o](const std::string &v, std::string &) {
    o.caPath = v;
    return true;
  });

  p.option("cert", 0, "FILE", "Client certificate (PEM)", [&o](const std::string &v, std::string &) {
    o.certFile = v;
    return true;
  });

  p.option("cert-key", 0, "FILE", "Private key for the client certificate (PEM)",
           [&o](const std::string &v, std::string &) {
             o.keyFile = v;
             return true;
           });

  p.flag("insecure", 0, "Do not verify the server certificate (testing only)", [&o]() { o.insecure = true; });

  p.flag("binary", 0, "tcpip: use the binary protocol", [&o]() { o.binary = true; });

  p.option("encryption", 'e', "ALG",
           "none, aes128, aes192 or aes256. udp/multicast: encrypt\n"
           "datagrams. tcpip: encrypted binary protocol.",
           [&o](const std::string &v, std::string &err) {
             std::string a = lower(v);
             if ("0" == a)
               a = "none";
             if ("1" == a)
               a = "aes128";
             if ("2" == a)
               a = "aes192";
             if ("3" == a)
               a = "aes256";
             if (("none" != a) && ("aes128" != a) && ("aes192" != a) && ("aes256" != a)) {
               err = "use none, aes128, aes192 or aes256";
               return false;
             }
             o.encryption = a;
             return true;
           });

  p.option("key", 'k', "HEX",
           "AES key in hex (or set VSCP_KEY). ws1/ws2: the\n"
           "authentication key.",
           [&o](const std::string &v, std::string &) {
             o.key = v;
             return true;
           });

  p.option("hmac-key", 0, "HEX", "HMAC-SHA256 key, 64 hex digits (or set VSCP_HMAC_KEY)",
           [&o](const std::string &v, std::string &) {
             o.hmacKey = v;
             return true;
           });

  p.flag("require-encryption", 0, "udp/multicast: drop received events that are not\nencrypted",
         [&o]() { o.requireEncryption = true; });

  p.group("MQTT");

  p.option("subscribe", 0, "TOPIC", "Topic to receive events on (default vscp/#).\nRepeatable.",
           [&o](const std::string &v, std::string &) {
             o.subscribe.push_back(v);
             return true;
           });

  p.option("publish", 0, "TOPIC",
           "Topic to send events to (default\n"
           "vscp/{{guid}}/{{class}}/{{type}}). Repeatable.",
           [&o](const std::string &v, std::string &) {
             o.publish.push_back(v);
             return true;
           });

  p.option("mqtt-format", 0, "FMT", "Publish format: json (default), xml, string or binary",
           [&o](const std::string &v, std::string &) {
             std::string f = lower(v);
             if (("json" != f) && ("xml" != f) && ("string" != f) && ("binary" != f)) {
               return false;
             }
             o.mqttFormat = f;
             return true;
           });

  p.option("qos", 0, "N", "MQTT QoS 0-2 (default 0)", [&o](const std::string &v, std::string &) {
    uint64_t n;
    if (!parseUnsigned(v, 2, n)) {
      return false;
    }
    o.qos = (int) n;
    return true;
  });

  p.group("Diagnostics");

  p.flag("verbose", 'v', "More log output on stderr (repeat for more)", [&o]() { o.verbose++; });

  p.flag("print-config", 0, "Print the interface configuration as JSON and exit", [&o]() { o.printConfig = true; });
}

///////////////////////////////////////////////////////////////////////////////
// finishConnectionOptions
//

void
finishConnectionOptions(ConnectionOptions &o)
{
  platformInit();

  const char *env;
  if (!o.passwordSet && (nullptr != (env = getenv("VSCP_PASSWORD")))) {
    o.password    = env;
    o.passwordSet = true;
  }
  if (o.key.empty() && (nullptr != (env = getenv("VSCP_KEY")))) {
    o.key = env;
  }
  if (o.hmacKey.empty() && (nullptr != (env = getenv("VSCP_HMAC_KEY")))) {
    o.hmacKey = env;
  }
  setupLogging(o.verbose);
}

///////////////////////////////////////////////////////////////////////////////
// interfaceName
//

std::string
interfaceName(const std::string &name)
{
  std::string n = lower(trim(name));
  if (("tcpip" == n) || ("tcp" == n)) {
    return "tcpip";
  }
  if (("multicast" == n) || ("mc" == n)) {
    return "multicast";
  }
  if (("socketcan" == n) || ("can" == n)) {
    return "socketcan";
  }
  if (("level2" == n) || ("l2" == n)) {
    return "level2";
  }
  if (("udp" == n) || ("ws1" == n) || ("ws2" == n) || ("mqtt" == n) || ("canal" == n)) {
    return n;
  }
  return "";
}

// Encryption name -> VSCP_ENCRYPTION_xxx
static int
encryptionCode(const std::string &alg)
{
  if ("aes128" == alg)
    return VSCP_ENCRYPTION_AES128;
  if ("aes192" == alg)
    return VSCP_ENCRYPTION_AES192;
  if ("aes256" == alg)
    return VSCP_ENCRYPTION_AES256;
  return VSCP_ENCRYPTION_NONE;
}

// IPv6 addresses need brackets in a URL
static std::string
urlHost(const std::string &h)
{
  return ((std::string::npos != h.find(':')) && ('[' != h[0])) ? ("[" + h + "]") : h;
}

// Replace the scheme of an URL ("tcp://x" -> "stcp://x")
static std::string
withScheme(const std::string &url, const std::string &scheme)
{
  size_t pos = url.find("://");
  return scheme + "://" + ((std::string::npos == pos) ? url : url.substr(pos + 3));
}

///////////////////////////////////////////////////////////////////////////////
// buildClientConfig
//

bool
buildClientConfig(ConnectionOptions &o, json &cfg, std::string &error)
{
  cfg = json::object();

  // Settings file. Either the client settings themselves or (as for the
  // simulator) an object with them in "interface".
  if (!o.configFile.empty()) {
    std::ifstream in(o.configFile);
    if (!in) {
      error = "Unable to open the configuration file " + o.configFile;
      return false;
    }
    try {
      in >> cfg;
    }
    catch (const std::exception &e) {
      error = "Invalid JSON in " + o.configFile + ": " + e.what();
      return false;
    }
    if (cfg.contains("interface") && cfg["interface"].is_object()) {
      json sub = cfg["interface"];
      cfg      = sub;
    }
    if (!cfg.is_object()) {
      error = o.configFile + " must hold a JSON object";
      return false;
    }
    if (o.iface.empty() && cfg.contains("type") && cfg["type"].is_string()) {
      o.iface = interfaceName(cfg["type"].get<std::string>());
      if (o.iface.empty()) {
        error = "Unknown interface type in " + o.configFile;
        return false;
      }
    }
    cfg.erase("type");
  }

  if (o.iface.empty()) {
    error = "No interface given. Use -i tcpip|udp|multicast|ws1|ws2|mqtt|socketcan|canal|level2";
    return false;
  }

  const std::string &t = o.iface;
  const bool tlsCapable = ("tcpip" == t) || ("ws1" == t) || ("ws2" == t) || ("mqtt" == t);
  const bool datagram   = ("udp" == t) || ("multicast" == t);
  const bool hasCertOpt = !o.caFile.empty() || !o.caPath.empty() || !o.certFile.empty() || !o.keyFile.empty();

  if ((o.tls || hasCertOpt || o.insecure) && !tlsCapable) {
    error = "TLS is not available for the " + t + " interface (use tcpip, ws1, ws2 or mqtt)";
    return false;
  }
  if ((!o.encryption.empty() || !o.hmacKey.empty() || o.requireEncryption) && !datagram && ("tcpip" != t)) {
    error = "Encryption options are for udp, multicast and tcpip. ws1/ws2 use --key for authentication.";
    return false;
  }
  if (!o.key.empty() && !datagram && ("tcpip" != t) && ("ws1" != t) && ("ws2" != t)) {
    error = "--key is not used by the " + t + " interface";
    return false;
  }
  if (!o.netInterface.empty() && !datagram) {
    error = "--net-interface is for the udp and multicast interfaces";
    return false;
  }
  if (o.binary && ("tcpip" != t)) {
    error = "--binary is only for the tcpip interface";
    return false;
  }
  if (o.requireEncryption && !datagram) {
    error = "--require-encryption is for udp and multicast";
    return false;
  }
  if ((!o.subscribe.empty() || !o.publish.empty() || !o.mqttFormat.empty() || (o.qos >= 0)) && ("mqtt" != t)) {
    error = "--subscribe, --publish, --mqtt-format and --qos are for the mqtt interface";
    return false;
  }
  if (!o.device.empty() && ("socketcan" != t)) {
    error = "--device is for the socketcan interface";
    return false;
  }
  if ((!o.driver.empty() || !o.driverConfig.empty()) && ("canal" != t) && ("level2" != t)) {
    error = "--driver and --driver-config are for the canal and level2 interfaces";
    return false;
  }

  auto setTls = [&](bool verifyKey) {
    if (!o.caFile.empty())
      cfg["cafile"] = o.caFile;
    if (!o.caPath.empty())
      cfg["capath"] = o.caPath;
    if (!o.certFile.empty())
      cfg["certfile"] = o.certFile;
    if (!o.keyFile.empty())
      cfg["keyfile"] = o.keyFile;
    if (verifyKey && o.insecure)
      cfg["bverifypeer"] = false;
  };

  // Encryption settings that are given must be complete
  std::string alg = o.encryption;
  if (alg.empty() && (!o.hmacKey.empty() || (!o.key.empty() && ("tcpip" == t || datagram)))) {
    error = "Give the algorithm with --encryption aes128|aes192|aes256 together with the keys";
    return false;
  }
  if (!alg.empty() && ("none" != alg)) {
    size_t keyLen = (VSCP_ENCRYPTION_AES128 == encryptionCode(alg)) ? 16 : ((VSCP_ENCRYPTION_AES192 == encryptionCode(alg)) ? 24 : 32);
    std::vector<uint8_t> tmp;
    if (o.key.empty() || !parseHexString(o.key, tmp) || (tmp.size() != keyLen)) {
      error = alg + " needs --key with " + std::to_string(keyLen * 2) + " hex digits";
      return false;
    }
    if (!o.hmacKey.empty() && (!parseHexString(o.hmacKey, tmp) || (32 != tmp.size()))) {
      error = "--hmac-key must be 64 hex digits (32 bytes)";
      return false;
    }
    if (("tcpip" == t) && o.hmacKey.empty()) {
      error = "The encrypted tcpip binary protocol needs both --key and --hmac-key";
      return false;
    }
  }

  if ("tcpip" == t) {
    if (!o.host.empty() || o.port || !cfg.contains("host")) {
      cfg["host"] = std::string(o.tls ? "stcp" : "tcp") + "://" + urlHost(o.host.empty() ? "localhost" : o.host) + ":" +
                    std::to_string(o.port ? o.port : VSCP_DEFAULT_TCP_PORT);
    }
    else if (o.tls) {
      cfg["host"] = withScheme(cfg["host"].get<std::string>(), "stcp");
    }
    if (o.tls) {
      cfg["btls"]     = true;
      cfg["tls-mode"] = "force-tls";
    }
    setTls(true);
    if (!o.user.empty() || !cfg.contains("user"))
      cfg["user"] = o.user.empty() ? "admin" : o.user;
    if (o.passwordSet || !cfg.contains("password"))
      cfg["password"] = o.passwordSet ? o.password : (o.user.empty() ? "secret" : "");
    if (o.binary || (!alg.empty() && ("none" != alg))) {
      cfg["binary-mode"] = true;
    }
    if (!alg.empty()) {
      cfg["binary-encryption"] = encryptionCode(alg);
      if ("none" != alg) {
        cfg["binary-key"]      = o.key;
        cfg["binary-hmac-key"] = o.hmacKey;
      }
    }
    cfg["connection-timeout"] = o.connectTimeout;
    cfg["response-timeout"]   = o.responseTimeout;
  }
  else if (datagram) {
    if (!o.host.empty()) {
      cfg["ip"] = o.host;
    }
    if (o.port) {
      cfg["port"] = o.port;
    }
    if (!o.netInterface.empty()) {
      cfg["interface"] = o.netInterface;
    }
    if (!alg.empty()) {
      cfg["encryption"] = encryptionCode(alg);
      if ("none" != alg) {
        cfg["key"] = o.key;
        if (!o.hmacKey.empty()) {
          cfg["hmac-key"] = o.hmacKey;
        }
      }
    }
    if (o.requireEncryption) {
      cfg["require-encryption"] = true;
    }
  }
  else if (("ws1" == t) || ("ws2" == t)) {
    if (!o.host.empty() || o.port || !cfg.contains("url")) {
      cfg["url"] = std::string(o.tls ? "wss" : "ws") + "://" + urlHost(o.host.empty() ? "localhost" : o.host) + ":" +
                   std::to_string(o.port ? o.port : 8884) + "/" + t;
    }
    else if (o.tls) {
      cfg["url"] = withScheme(cfg["url"].get<std::string>(), "wss");
    }
    setTls(true);
    if (!o.user.empty() || !cfg.contains("user"))
      cfg["user"] = o.user.empty() ? "admin" : o.user;
    if (o.passwordSet || !cfg.contains("password"))
      cfg["password"] = o.passwordSet ? o.password : (o.user.empty() ? "secret" : "");
    if (!o.key.empty()) {
      std::vector<uint8_t> tmp;
      if (!parseHexString(o.key, tmp) || (16 != tmp.size())) {
        error = "The websocket authentication key (--key) must be 32 hex digits";
        return false;
      }
      cfg["key"] = o.key;
    }
    cfg["connection-timeout"] = o.connectTimeout;
    cfg["response-timeout"]   = o.responseTimeout;
  }
  else if ("mqtt" == t) {
    if (!o.host.empty() || !cfg.contains("host")) {
      cfg["host"] = o.host.empty() ? "localhost" : o.host;
    }
    if (o.port || !cfg.contains("port")) {
      cfg["port"] = o.port ? o.port : (o.tls ? 8883 : 1883);
    }
    if (o.tls || hasCertOpt) {
      // The MQTT client only uses TLS with a CA to verify the broker
      json tls = cfg.contains("tls") && cfg["tls"].is_object() ? cfg["tls"] : json::object();
      if (!o.caFile.empty())
        tls["cafile"] = o.caFile;
      if (!o.caPath.empty())
        tls["capath"] = o.caPath;
      if (!o.certFile.empty())
        tls["certfile"] = o.certFile;
      if (!o.keyFile.empty())
        tls["keyfile"] = o.keyFile;
      if (o.insecure) {
        tls["no-hostname-checking"] = true;
        tls["cert-reqs"]            = 0;
      }
      if (!tls.contains("cafile") && !tls.contains("capath")) {
        error = "MQTT over TLS needs --ca-file or --ca-path (for example --ca-path /etc/ssl/certs)";
        return false;
      }
      if (tls.contains("certfile") != tls.contains("keyfile")) {
        error = "Give both --cert and --cert-key for a client certificate";
        return false;
      }
      cfg["tls"] = tls;
    }
    if (!o.user.empty()) {
      cfg["user"] = o.user;
    }
    if (o.passwordSet) {
      cfg["password"] = o.password;
    }
    int qos = (o.qos >= 0) ? o.qos : 0;
    if (!o.subscribe.empty() || !cfg.contains("subscribe")) {
      json subs = json::array();
      for (const auto &topic : (o.subscribe.empty() ? std::vector<std::string>{ "vscp/#" } : o.subscribe)) {
        subs.push_back({ { "topic", topic }, { "qos", qos }, { "format", "auto" } });
      }
      cfg["subscribe"] = subs;
    }
    if (!o.publish.empty() || !cfg.contains("publish")) {
      json pubs = json::array();
      for (const auto &topic :
           (o.publish.empty() ? std::vector<std::string>{ "vscp/{{guid}}/{{class}}/{{type}}" } : o.publish)) {
        pubs.push_back({ { "topic", topic },
                         { "qos", qos },
                         { "bretain", false },
                         { "format", o.mqttFormat.empty() ? "json" : o.mqttFormat } });
      }
      cfg["publish"] = pubs;
    }
    cfg["connection-timeout"] = o.connectTimeout;
  }
  else if ("canal" == t) {
    if (!o.driver.empty())
      cfg["path"] = o.driver;
    if (!o.driverConfig.empty() || !cfg.contains("config"))
      cfg["config"] = o.driverConfig;
    if (o.flagsSet || !cfg.contains("flags"))
      cfg["flags"] = o.flags;
    if (!cfg.contains("name"))
      cfg["name"] = "vscp-cli";
    if (!cfg.contains("path")) {
      error = "The canal interface needs the driver library (--driver PATH)";
      return false;
    }
  }
  else if ("level2" == t) {
    if (!o.driver.empty())
      cfg["path-driver"] = o.driver;
    if (!o.driverConfig.empty())
      cfg["path-config"] = o.driverConfig;
    if (!o.guid.empty())
      cfg["guid"] = o.guid;
    if (!cfg.contains("name"))
      cfg["name"] = "vscp-cli";
    if (!cfg.contains("path-driver")) {
      error = "The level2 interface needs the driver library (--driver PATH)";
      return false;
    }
  }
  else if ("socketcan" == t) {
#ifndef VSCPCLI_HAVE_SOCKETCAN
    error = "SocketCAN is only available on Linux";
    return false;
#endif
    if (!o.device.empty() || !cfg.contains("device"))
      cfg["device"] = o.device.empty() ? "vcan0" : o.device;
    if (o.flagsSet || !cfg.contains("flags"))
      cfg["flags"] = o.flags;
    cfg["response-timeout"] = o.responseTimeout;
  }

  // --set has the last word
  for (const auto &kv : o.sets) {
    cfg[kv.first] = kv.second;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// handlePrintConfig
//

bool
handlePrintConfig(ConnectionOptions &o, int &exitCode)
{
  if (!o.printConfig) {
    return false;
  }

  json cfg;
  std::string error;
  if (!buildClientConfig(o, cfg, error)) {
    fprintf(stderr, "%s\n", error.c_str());
    exitCode = EXIT_USAGE;
    return true;
  }

  // Don't show secrets
  for (const char *secret : { "password", "key", "hmac-key", "binary-key", "binary-hmac-key" }) {
    if (cfg.contains(secret) && cfg[secret].is_string() && !cfg[secret].get<std::string>().empty()) {
      cfg[secret] = "********";
    }
  }

  printf("{\n  \"type\": \"%s\",\n", o.iface.c_str());
  std::string body = cfg.dump(2);
  printf("%s\n", body.substr(2).c_str()); // Continue the object
  exitCode = EXIT_OK;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// openClient
//

std::unique_ptr<CVscpClient>
openClient(ConnectionOptions &o, std::string &error)
{
  json cfg;
  o.configError = false;
  if (!buildClientConfig(o, cfg, error)) {
    o.configError = true;
    return nullptr;
  }

  std::unique_ptr<CVscpClient> client;
  const std::string &t = o.iface;

  if ("tcpip" == t) {
    client.reset(new vscpClientTcp());
  }
  else if ("udp" == t) {
    client.reset(new vscpClientUdp());
  }
  else if ("multicast" == t) {
    client.reset(new vscpClientMulticast());
  }
  else if ("ws1" == t) {
    client.reset(new vscpClientWs1());
  }
  else if ("ws2" == t) {
    client.reset(new vscpClientWs2());
  }
  else if ("mqtt" == t) {
    client.reset(new vscpClientMqtt());
  }
  else if ("canal" == t) {
    client.reset(new vscpClientCanal());
  }
  else if ("level2" == t) {
    client.reset(new vscpClientLevel2());
  }
#ifdef VSCPCLI_HAVE_SOCKETCAN
  else if ("socketcan" == t) {
    client.reset(new vscpClientSocketCan());
  }
#endif
  else {
    error = "Interface " + t + " is not available on this platform";
    return nullptr;
  }

  if (!client->initFromJson(cfg.dump())) {
    error         = "Invalid settings for the " + t + " interface (run with -v for details)";
    o.configError = true;
    return nullptr;
  }

#ifdef VSCPCLI_HAVE_SOCKETCAN
  if ("socketcan" == t) {
    // Received events get a GUID built from the interface GUID
    int rv = ((vscpClientSocketCan *) client.get())
               ->init(cfg["device"].get<std::string>(), o.guid.empty() ? "-" : o.guid,
                      cfg.value("flags", (unsigned long) 0), o.responseTimeout);
    if (VSCP_ERROR_SUCCESS != rv) {
      error = "Unable to set up SocketCAN device " + cfg["device"].get<std::string>() + ": " + errorText(rv);
      return nullptr;
    }
  }
#endif

  client->setConnectionTimeout(o.connectTimeout);
  client->setResponseTimeout(o.responseTimeout);

  int rv = client->connect();
  if (VSCP_ERROR_SUCCESS != rv) {
    error = "Unable to connect the " + t + " interface: " + errorText(rv);
    return nullptr;
  }

  // Some clients (MQTT) connect in the background
  uint64_t start = msNow();
  while (!client->isConnected()) {
    if (!g_run || ((msNow() - start) > o.connectTimeout)) {
      client->disconnect();
      error = "Timeout while connecting the " + t + " interface";
      return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  spdlog::info("Connected ({})", t);
  return client;
}

///////////////////////////////////////////////////////////////////////////////
// closeClient
//

void
closeClient(std::unique_ptr<CVscpClient> &client)
{
  if (client) {
    client->disconnect();
    client.reset();
  }
}

} // namespace vscpcli
