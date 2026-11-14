// cli-util.cpp
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include "cli-util.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <ctime>
#include <thread>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vscp.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace vscpcli {

std::atomic<bool> g_run{ true };

static void
signalHandler(int /*sig*/)
{
  g_run = false;
}

void
platformInit(void)
{
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void
installSignalHandlers(void)
{
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
}

void
setupLogging(int verbosity)
{
  auto logger = spdlog::stderr_color_mt("vscp");
  logger->set_pattern("%^[%l]%$ %v");
  spdlog::set_default_logger(logger);
  switch (verbosity) {
    case 0:
      spdlog::set_level(spdlog::level::warn);
      break;
    case 1:
      spdlog::set_level(spdlog::level::info);
      break;
    case 2:
      spdlog::set_level(spdlog::level::debug);
      break;
    default:
      spdlog::set_level(spdlog::level::trace);
      break;
  }
}

bool
parseUnsigned(const std::string &s, uint64_t max, uint64_t &value)
{
  std::string str = trim(s);
  if (str.empty()) {
    return false;
  }

  int base   = 10;
  size_t pos = 0;
  if ((str.size() > 2) && ('0' == str[0]) && (('x' == str[1]) || ('X' == str[1]))) {
    base = 16;
    pos  = 2;
  }

  uint64_t v = 0;
  for (; pos < str.size(); pos++) {
    int c = (unsigned char) str[pos];
    int digit;
    if (isdigit(c)) {
      digit = c - '0';
    }
    else if ((16 == base) && isxdigit(c)) {
      digit = tolower(c) - 'a' + 10;
    }
    else {
      return false;
    }
    if (v > (max - digit) / base) {
      return false; // Overflow / larger than max
    }
    v = v * base + digit;
  }

  value = v;
  return true;
}

bool
parseHexString(const std::string &s, std::vector<uint8_t> &bytes)
{
  std::string hex;
  for (char c : s) {
    if ((':' == c) || (' ' == c) || ('-' == c)) {
      continue;
    }
    if (!isxdigit((unsigned char) c)) {
      return false;
    }
    hex += c;
  }
  if (hex.size() % 2) {
    return false;
  }
  bytes.clear();
  for (size_t i = 0; i < hex.size(); i += 2) {
    bytes.push_back((uint8_t) std::stoul(hex.substr(i, 2), nullptr, 16));
  }
  return true;
}

bool
parseByteList(const std::string &str, std::vector<uint8_t> &bytes, std::string &error)
{
  for (const auto &token : split(str, ", \t")) {
    if (':' == token[0]) {
      std::vector<uint8_t> run;
      if (!parseHexString(token.substr(1), run)) {
        error = "'" + token + "' is not a run of hex pairs";
        return false;
      }
      bytes.insert(bytes.end(), run.begin(), run.end());
      continue;
    }
    uint64_t v;
    if (!parseUnsigned(token, 255, v)) {
      error = "'" + token + "' is not a byte (0-255 or 0x00-0xff)";
      return false;
    }
    bytes.push_back((uint8_t) v);
  }
  return true;
}

std::vector<std::string>
split(const std::string &str, const std::string &separators)
{
  std::vector<std::string> parts;
  std::string cur;
  for (char c : str) {
    if (std::string::npos != separators.find(c)) {
      if (!cur.empty()) {
        parts.push_back(cur);
      }
      cur.clear();
    }
    else {
      cur += c;
    }
  }
  if (!cur.empty()) {
    parts.push_back(cur);
  }
  return parts;
}

std::string
trim(const std::string &str)
{
  size_t b = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == b) {
    return "";
  }
  size_t e = str.find_last_not_of(" \t\r\n");
  return str.substr(b, e - b + 1);
}

std::string
lower(const std::string &str)
{
  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) tolower(c); });
  return s;
}

uint64_t
msNow(void)
{
  using namespace std::chrono;
  return (uint64_t) duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void
sleepMs(uint32_t ms)
{
  uint64_t end = msNow() + ms;
  while (g_run) {
    uint64_t now = msNow();
    if (now >= end) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((std::min<uint64_t>)(end - now, 50)));
  }
}

std::string
localTimeString(void)
{
  using namespace std::chrono;
  auto now  = system_clock::now();
  time_t t  = system_clock::to_time_t(now);
  int ms    = (int) (duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
  struct tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
  char out[48];
  snprintf(out, sizeof(out), "%s.%03d", buf, ms);
  return out;
}

std::string
errorText(int rv)
{
  switch (rv) {
    case VSCP_ERROR_SUCCESS:
      return "success";
    case VSCP_ERROR_TIMEOUT:
      return "timeout (no answer)";
    case VSCP_ERROR_NOT_SUPPORTED:
      return "not supported by this interface";
    case VSCP_ERROR_PARAMETER:
      return "invalid parameter";
    case VSCP_ERROR_CONNECTION:
      return "could not connect";
    case VSCP_ERROR_NOT_CONNECTED:
      return "not connected";
    case VSCP_ERROR_NOT_OPEN:
      return "interface not open";
    case VSCP_ERROR_USER:
      return "invalid user";
    case VSCP_ERROR_PASSWORD:
      return "invalid password";
    case VSCP_ERROR_COMMUNICATION:
      return "communication error";
    case VSCP_ERROR_HARDWARE:
      return "hardware error";
    case VSCP_ERROR_LIBRARY:
      return "unable to load the driver library";
    case VSCP_ERROR_PROCADDRESS:
      return "the driver library lacks a needed function";
    case VSCP_ERROR_TRM_FULL:
      return "transmit buffer full";
    case VSCP_ERROR_OPERATION_FAILED:
      return "operation failed";
    case VSCP_ERROR_INVALID_FRAME:
      return "invalid frame";
    case VSCP_ERROR_STOPPED:
      return "stopped";
    case VSCP_ERROR_INIT_FAIL:
      return "initialization failed";
    case VSCP_ERROR_ERROR:
      return "failed";
    default:
      return "error " + std::to_string(rv);
  }
}

} // namespace vscpcli
