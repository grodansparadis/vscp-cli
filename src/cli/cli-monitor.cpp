// SPDX-License-Identifier: MIT

#ifdef _WIN32
#include <pch.h>
#endif

#include "cli-monitor.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <cli-args.h>
#include <cli-connection.h>
#include <cli-event.h>
#include <cli-util.h>

#include <vscp-class.h>
#include <vscp-client-base.h>
#include <vscp-type.h>

namespace vscpcli {
namespace {

struct MonitorOptions {
  bool hex = false;
  bool quiet = false;
  std::string logFile;
  uint64_t count = 0;
  uint32_t idle = 0;
  uint32_t duration = 0;
  uint32_t readyTimeout = 500;
  size_t maxSize = 1024 * 1024;
  Guid from{};
  bool fromSet = false;
  uint8_t node = 0;
  bool nodeSet = false;
};

std::string
typeDescription(uint16_t cls, uint16_t type)
{
  static const struct {
    uint16_t cls;
    uint16_t type;
    const char *description;
  } descriptions[] = {
#include <vscp-monitor-types.inc>
  };
  for (const auto &entry : descriptions) {
    if ((entry.cls == cls) && (entry.type == type)) {
      return entry.description;
    }
  }
  return "Unknown type";
}

std::string
logLevel(uint8_t level)
{
  static const char *levels[] = {"Emergency", "Alert",         "Critical", "Error",  "Warning",
                                 "Notice",    "Informational", "Debug",    "Verbose"};
  return level < sizeof(levels) / sizeof(levels[0]) ? levels[level] : "Application-defined";
}

std::string
textData(const std::vector<uint8_t> &data)
{
  std::string text = "\"";
  for (uint8_t byte : data) {
    switch (byte) {
    case '\n':
      text += "\\n";
      break;
    case '\r':
      text += "\\r";
      break;
    case '\t':
      text += "\\t";
      break;
    case '\0':
      text += "\\0";
      break;
    case '\\':
      text += "\\\\";
      break;
    case '"':
      text += "\\\"";
      break;
    default:
      if ((byte >= 32) && (byte <= 126)) {
        text += static_cast<char>(byte);
      }
      else {
        char escaped[5];
        snprintf(escaped, sizeof(escaped), "\\x%02X", byte);
        text += escaped;
      }
    }
  }
  return text + "\"";
}

class Collector {
public:
  Collector(MonitorKind kind, const MonitorOptions &options, FILE *log)
      : m_kind(kind), m_options(options), m_log(log)
  {
  }

  bool done() const { return m_options.count && (m_printed >= m_options.count); }
  bool pending() const { return !m_pending.empty(); }

  bool expire(uint64_t now, uint32_t &wait, std::string &error)
  {
    for (auto it = m_pending.begin(); it != m_pending.end() && !done();) {
      uint64_t elapsed = now - it->second.last;
      if (elapsed >= m_options.readyTimeout) {
        auto ready = it++;
        if (!flush(ready, error)) {
          return false;
        }
      }
      else {
        wait = std::min<uint32_t>(wait, static_cast<uint32_t>(m_options.readyTimeout - elapsed));
        ++it;
      }
    }
    return true;
  }

  bool receive(const vscpEventEx &ex, uint64_t now, std::string &error)
  {
    Guid guid;
    std::copy(ex.GUID, ex.GUID + guid.size(), guid.begin());
    std::string origin = localTimeString() + " " + formatGuid(guid) + " ";
    if (ex.sizeData > 8) {
      error = origin + "Level I payload exceeds 8 bytes";
      return false;
    }

    if ((MonitorKind::Diagnostic == m_kind) || (MonitorKind::Error == m_kind)) {
      if (ex.sizeData < 3) {
        error = origin + "diagnostic/error payload needs index, zone and subzone";
        return false;
      }
      return write(origin + (MonitorKind::Diagnostic == m_kind ? "CLASS1.DIAGNOSTIC" : "CLASS1.ERROR") +
                       " type=" + std::to_string(ex.vscp_type) + " (" +
                       typeDescription(ex.vscp_class, ex.vscp_type) +
                       ") index=" + std::to_string(ex.data[0]) + " zone=" + std::to_string(ex.data[1]) +
                       " subzone=" + std::to_string(ex.data[2]) +
                       (ex.sizeData > 3 ? " data=" + dataHex(ex.data + 3, ex.sizeData - 3) : ""),
                   error);
    }

    if (MonitorKind::Stream == m_kind) {
      if (!ex.sizeData) {
        error = origin + "stream payload needs a sequence number";
        return false;
      }
      return fragment({guid, 0}, ex.data[0], 0, ex.data + 1, ex.sizeData - 1, now, error);
    }

    if (VSCP_TYPE_LOG_MESSAGE == ex.vscp_type) {
      if (ex.sizeData < 3) {
        error = origin + "log message needs channel, level and fragment index";
        return false;
      }
      return fragment({guid, ex.data[0]}, ex.data[2], ex.data[1], ex.data + 3, ex.sizeData - 3, now, error);
    }

    std::string label;
    switch (ex.vscp_type) {
    case VSCP_TYPE_LOG_GENERAL:
      label = "General log event";
      break;
    case VSCP_TYPE_LOG_START:
      label = "Log start";
      break;
    case VSCP_TYPE_LOG_STOP:
      label = "Log stop";
      break;
    case VSCP_TYPE_LOG_LEVEL:
      label = "Log level";
      break;
    default:
      label = "Unknown log type";
      break;
    }
    bool channelEvent = (VSCP_TYPE_LOG_START == ex.vscp_type) || (VSCP_TYPE_LOG_STOP == ex.vscp_type);
    if ((channelEvent || (VSCP_TYPE_LOG_LEVEL == ex.vscp_type)) && !ex.sizeData) {
      error = origin + label + (channelEvent ? " needs a channel ID" : " needs level data");
      return false;
    }
    if ((VSCP_TYPE_LOG_START == ex.vscp_type) || (VSCP_TYPE_LOG_STOP == ex.vscp_type)) {
      auto it = m_pending.find({guid, ex.data[0]});
      if ((it != m_pending.end()) && !flush(it, error)) {
        return false;
      }
      if (done()) {
        return true;
      }
    }
    return write(origin + "CLASS1.LOG type=" + std::to_string(ex.vscp_type) + " (" + label + ")" +
                     (channelEvent ? " channel=" + std::to_string(ex.data[0]) : "") +
                     (ex.sizeData ? " data=" + dataHex(ex) : ""),
                 error);
  }

private:
  using Key = std::pair<Guid, uint8_t>;
  struct Message {
    std::vector<uint8_t> data;
    uint16_t next = 0;
    uint8_t level = 0;
    uint64_t last = 0;
  };
  using Messages = std::map<Key, Message>;

  bool write(const std::string &line, std::string &error)
  {
    if (!m_options.quiet && ((fprintf(stdout, "%s\n", line.c_str()) < 0) || (fflush(stdout) != 0))) {
      error = "unable to write stdout";
      return false;
    }
    if (m_log && ((fprintf(m_log, "%s\n", line.c_str()) < 0) || (fflush(m_log) != 0))) {
      error = "unable to write log file";
      return false;
    }
    ++m_printed;
    return true;
  }

  bool flush(Messages::iterator it, std::string &error)
  {
    const auto &message = it->second;
    std::string line = localTimeString() + " " + formatGuid(it->first.first);
    if (MonitorKind::Stream == m_kind) {
      line += " stream";
    }
    else {
      line += " log channel=" + std::to_string(it->first.second) + " level=" + std::to_string(message.level) +
              " (" + logLevel(message.level) + ")";
    }
    line += " bytes=" + std::to_string(message.data.size());
    line += m_options.hex ? " hex=" + dataHex(message.data.data(), message.data.size())
                          : " text=" + textData(message.data);
    if (!write(line, error)) {
      return false;
    }
    m_buffered -= message.data.size();
    m_pending.erase(it);
    return true;
  }

  bool fragment(const Key &key, uint8_t sequence, uint8_t level, const uint8_t *data, size_t size,
                uint64_t now, std::string &error)
  {
    auto it = m_pending.find(key);
    // Stream zero can be a new start or the wrap after 255; log zero always starts a message.
    if ((it != m_pending.end()) && !sequence && ((MonitorKind::Log == m_kind) || it->second.next)) {
      if (!flush(it, error)) {
        return false;
      }
      if (done()) {
        return true;
      }
      it = m_pending.end();
    }
    uint16_t expected = it == m_pending.end() ? 0 : it->second.next;
    if (sequence != expected) {
      error = formatGuid(key.first) + " channel=" + std::to_string(key.second) +
              ": invalid fragment sequence (expected index " + std::to_string(expected) + ", received " +
              std::to_string(sequence) + "); incomplete data discarded";
      return false;
    }
    if ((it != m_pending.end()) && (it->second.level != level)) {
      error = formatGuid(key.first) + " channel=" + std::to_string(key.second) +
              ": log level changed within message (expected " + std::to_string(it->second.level) +
              ", received " + std::to_string(level) + "); incomplete data discarded";
      return false;
    }
    if (size > m_options.maxSize - m_buffered) {
      error = "buffer limit (--max-size) exceeded; incomplete data discarded";
      return false;
    }
    if (it == m_pending.end()) {
      if (m_pending.size() >= 1024) {
        error = "too many unfinished messages (maximum 1024); narrow the node selection";
        return false;
      }
      it = m_pending.emplace(key, Message{}).first;
      it->second.level = level;
    }
    auto &message = it->second;
    message.data.insert(message.data.end(), data, data + size);
    message.next = MonitorKind::Stream == m_kind ? static_cast<uint8_t>(sequence + 1) : sequence + 1;
    message.last = now;
    m_buffered += size;
    return true;
  }

  MonitorKind m_kind;
  const MonitorOptions &m_options;
  FILE *m_log;
  uint64_t m_printed = 0;
  size_t m_buffered = 0;
  Messages m_pending;
};

} // namespace

int
runMonitor(int argc, char **argv, MonitorKind kind)
{
  const char *name;
  const char *description;
  uint16_t cls;
  switch (kind) {
  case MonitorKind::Stream:
    name = "vscp-stream";
    description = "Collect CLASS1.INFORMATION Stream Data (type 36) as text or hexadecimal bytes.";
    cls = VSCP_CLASS1_INFORMATION;
    break;
  case MonitorKind::Diagnostic:
    name = "vscp-diagnostic";
    description = "Show CLASS1.DIAGNOSTIC events as readable descriptions.";
    cls = VSCP_CLASS1_DIAGNOSTIC;
    break;
  case MonitorKind::Error:
    name = "vscp-error";
    description = "Show CLASS1.ERROR events as readable descriptions.";
    cls = VSCP_CLASS1_ERROR;
    break;
  case MonitorKind::Log:
    name = "vscp-log";
    description = "Collect CLASS1.LOG messages with channel IDs and readable log levels.";
    cls = VSCP_CLASS1_LOG;
    break;
  default:
    fprintf(stderr, "invalid monitor kind\n");
    return EXIT_USAGE;
  }

  MonitorOptions options;
  ConnectionOptions conn;
  ArgParser parser(name, "[OPTIONS]", description);
  parser.group("Output");
  if (MonitorKind::Stream == kind) {
    parser.option("format", 'f', "FMT", "text (default), hex or binary (hexadecimal byte dump)",
                  [&](const std::string &v, std::string &) {
                    std::string value = lower(v);
                    if ((value != "text") && (value != "hex") && (value != "binary")) {
                      return false;
                    }
                    options.hex = value != "text";
                    return true;
                  });
  }
  parser.option("log", 'l', "FILE", "Also append output to FILE", [&](const std::string &v, std::string &) {
    options.logFile = v;
    return true;
  });
  parser.flag("quiet", 'q', "No stdout output (use with --log or --count)", [&]() { options.quiet = true; });

  parser.group("Selection");
  parser.option("from", 0, "GUID", "Only this full GUID (all supported VSCP GUID forms)",
                [&](const std::string &v, std::string &) {
                  options.fromSet = parseGuid(v, options.from);
                  return options.fromSet;
                });
  parser.option("node", 'N', "NICKNAME", "Only this node nickname, 0-255 (last GUID byte)",
                [&](const std::string &v, std::string &) {
                  uint64_t n;
                  if (!parseUnsigned(v, 255, n)) {
                    return false;
                  }
                  options.node = static_cast<uint8_t>(n);
                  options.nodeSet = true;
                  return true;
                });

  auto milliseconds = [](const std::string &v, uint32_t &target, bool positive) {
    uint64_t n;
    if (!parseUnsigned(v, UINT32_MAX, n) || (positive && !n)) {
      return false;
    }
    target = static_cast<uint32_t>(n);
    return true;
  };
  if ((MonitorKind::Stream == kind) || (MonitorKind::Log == kind)) {
    parser.group("Assembly");
    parser.option(
        "ready-timeout", 0, "MS", "Finish a message after MS ms of inactivity (default 500, minimum 1)",
        [&](const std::string &v, std::string &) { return milliseconds(v, options.readyTimeout, true); });
    parser.option("max-size", 0, "BYTES", "Maximum total buffered payload (default 1048576, minimum 1)",
                  [&](const std::string &v, std::string &) {
                    uint64_t n;
                    if (!parseUnsigned(v, (std::numeric_limits<size_t>::max)(), n) || !n) {
                      return false;
                    }
                    options.maxSize = static_cast<size_t>(n);
                    return true;
                  });
  }
  parser.group("Stop");
  parser.option(
      "count", 'n', "N", "Exit after N output records, not fragments (0 = unlimited)",
      [&](const std::string &v, std::string &) { return parseUnsigned(v, UINT64_MAX, options.count); });
  parser.option("idle", 0, "MS", "Exit after MS ms without selected events (0 = unlimited)",
                [&](const std::string &v, std::string &) { return milliseconds(v, options.idle, false); });
  parser.option(
      "duration", 0, "MS", "Exit after MS ms (0 = unlimited)",
      [&](const std::string &v, std::string &) { return milliseconds(v, options.duration, false); });
  addConnectionOptions(parser, conn);
  std::string epilog =
      "Without --from or --node, collect from all nodes. Both selectors together must match.\n";
  if ((MonitorKind::Stream == kind) || (MonitorKind::Log == kind)) {
    epilog += "Stream/log assembly starts at index 0. Missing/out-of-order fragments are errors.\n"
              "Use --idle/--duration longer than --ready-timeout to allow pending data to finish.";
  }
  parser.epilog(epilog);

  std::vector<std::string> positional;
  std::string error;
  bool exitNow;
  if (!parser.parse(argc, argv, positional, error, exitNow)) {
    fprintf(stderr, "%s: %s\n", name, error.c_str());
    return EXIT_USAGE;
  }
  if (exitNow) {
    return EXIT_OK;
  }
  if (!positional.empty()) {
    fprintf(stderr, "%s: unexpected argument '%s'\n", name, positional[0].c_str());
    return EXIT_USAGE;
  }
  finishConnectionOptions(conn);
  int result;
  if (handlePrintConfig(conn, result)) {
    return result;
  }

  auto closeLog = [](FILE *file) { fclose(file); };
  std::unique_ptr<FILE, decltype(closeLog)> log(nullptr, closeLog);
  if (!options.logFile.empty()) {
    log.reset(fopen(options.logFile.c_str(), "a"));
    if (!log) {
      fprintf(stderr, "%s: unable to open %s: %s\n", name, options.logFile.c_str(), strerror(errno));
      return EXIT_USAGE;
    }
  }
  installSignalHandlers();
  auto client = openClient(conn, error);
  if (!client) {
    fprintf(stderr, "%s: %s\n", name, error.c_str());
    return conn.configError ? EXIT_USAGE : EXIT_CONNECTION;
  }

  Collector collector(kind, options, log.get());
  uint64_t start = msNow();
  uint64_t lastEvent = start;
  result = EXIT_OK;
  while (g_run && !collector.done()) {
    uint64_t now = msNow();
    uint32_t wait = 100;
    if ((options.duration && (now - start >= options.duration)) ||
        (options.idle && (now - lastEvent >= options.idle))) {
      break;
    }
    if (!collector.expire(now, wait, error)) {
      result = EXIT_ERROR;
      break;
    }
    if (collector.done()) {
      break;
    }
    if (options.duration) {
      wait = std::min<uint32_t>(wait, static_cast<uint32_t>(options.duration - (now - start)));
    }
    if (options.idle) {
      wait = std::min<uint32_t>(wait, static_cast<uint32_t>(options.idle - (now - lastEvent)));
    }

    vscpEventEx ex{};
    int rv = client->receiveBlocking(ex, wait);
    if (VSCP_ERROR_SUCCESS != rv) {
      if (!client->isConnected()) {
        error = "connection lost";
        result = EXIT_CONNECTION;
        break;
      }
      if ((VSCP_ERROR_TIMEOUT == rv) || (VSCP_ERROR_FIFO_EMPTY == rv) || (VSCP_ERROR_RCV_EMPTY == rv)) {
        continue;
      }
      error = "receive failed: " + errorText(rv);
      result = EXIT_ERROR;
      break;
    }
    if ((ex.vscp_class != cls) ||
        ((MonitorKind::Stream == kind) && (ex.vscp_type != VSCP_TYPE_INFORMATION_STREAM_DATA)) ||
        (options.fromSet && memcmp(ex.GUID, options.from.data(), options.from.size())) ||
        (options.nodeSet && (ex.GUID[15] != options.node))) {
      continue;
    }
    lastEvent = msNow();
    if (!collector.receive(ex, lastEvent, error)) {
      result = EXIT_ERROR;
      break;
    }
  }

  if (EXIT_OK != result) {
    fprintf(stderr, "%s: %s\n", name, error.c_str());
  }
  else {
    if (collector.pending()) {
      fprintf(stderr, "%s: unfinished stream/log data discarded before ready timeout\n", name);
      if (g_run && !collector.done()) {
        result = EXIT_TIMEOUT;
      }
    }
    if (options.count && !collector.done() && g_run) {
      result = EXIT_TIMEOUT;
    }
  }
  closeClient(client);
  if (log && (fclose(log.release()) != 0)) {
    fprintf(stderr, "%s: unable to close log file: %s\n", name, strerror(errno));
    result = EXIT_ERROR;
  }
  return result;
}

} // namespace vscpcli
