// cli-util.h
//
// Helpers shared by the VSCP command line tools
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifndef VSCPCLI_UTIL_H
#define VSCPCLI_UTIL_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace vscpcli {

// Exit codes used by all tools
enum ExitCode {
  EXIT_OK         = 0,
  EXIT_ERROR      = 1, // Operation failed
  EXIT_USAGE      = 2, // Command line error
  EXIT_CONNECTION = 3, // Could not connect
  EXIT_TIMEOUT    = 4, // No answer from the node
};

/*!
  Cleared by Ctrl+C (SIGINT) and SIGTERM. Loops in the tools check it.
*/
extern std::atomic<bool> g_run;

/*!
  Process setup (Winsock on Windows). Called by finishConnectionOptions.
*/
void
platformInit(void);

/*!
  Install the Ctrl+C / SIGTERM handlers that clear g_run
*/
void
installSignalHandlers(void);

/*!
  Log library messages to stderr: 0 = warnings and errors, 1 = info,
  2 = debug, 3 = trace. Keeps stdout clean for the tool output.
*/
void
setupLogging(int verbosity);

/*!
  Parse an unsigned number, decimal or hex with 0x prefix.
  @return false if it is not a number or larger than max.
*/
bool
parseUnsigned(const std::string &str, uint64_t max, uint64_t &value);

/*!
  Parse a list of bytes separated by commas and/or spaces. Each byte is
  decimal or hex with 0x prefix. A token with only hex digits after a
  leading ':' is a run of hex pairs (":0a0b0c").
*/
bool
parseByteList(const std::string &str, std::vector<uint8_t> &bytes, std::string &error);

/*!
  Parse a hex string (pairs, optional ':' or space separators) into bytes
*/
bool
parseHexString(const std::string &str, std::vector<uint8_t> &bytes);

/*!
  Split a string on any of the characters in separators. Empty parts are
  dropped.
*/
std::vector<std::string>
split(const std::string &str, const std::string &separators);

/*!
  Trim white space on both ends
*/
std::string
trim(const std::string &str);

/*!
  Lower case copy
*/
std::string
lower(const std::string &str);

/*!
  Milliseconds since an arbitrary start (monotonic)
*/
uint64_t
msNow(void);

/*!
  Sleep that returns early when g_run is cleared
*/
void
sleepMs(uint32_t ms);

/*!
  Local time as YYYY-MM-DDTHH:MM:SS.mmm
*/
std::string
localTimeString(void);

/*!
  Text for a VSCP error code
*/
std::string
errorText(int rv);

} // namespace vscpcli

#endif
