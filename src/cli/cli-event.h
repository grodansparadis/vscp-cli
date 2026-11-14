// cli-event.h
//
// Event input and output formats for the VSCP command line tools
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifndef VSCPCLI_EVENT_H
#define VSCPCLI_EVENT_H

#include <cstdint>
#include <string>

#include <vscp.h>

namespace vscpcli {

enum class OutputFormat { TEXT, STRING, JSON, XML, CSV };

enum class TimeMode {
  ABSOLUTE, // Local wall clock time when received
  DELTA,    // Time since the previous event
  ZERO,     // Time since the first event
  NONE
};

bool
parseOutputFormat(const std::string &str, OutputFormat &fmt);

bool
parseTimeMode(const std::string &str, TimeMode &mode);

/*!
  Formats received events, one line each
*/
class EventFormatter {
public:
  EventFormatter(OutputFormat fmt, TimeMode tm)
    : m_fmt(fmt)
    , m_time(tm)
  {
  }

  /*!
    Header line for formats that have one (CSV), empty otherwise
  */
  std::string header(void) const;

  /*!
    The event as one line (without newline)
  */
  std::string format(const vscpEventEx &ex);

private:
  std::string timeField(void);

  OutputFormat m_fmt;
  TimeMode m_time;
  uint64_t m_first = 0;
  uint64_t m_last  = 0;
};

/*!
  Parse an event from a line: JSON ('{'), XML ('<') or the VSCP string
  format head,class,type,obid,datetime,timestamp,GUID,data,...
*/
bool
parseEventLine(const std::string &line, vscpEventEx &ex, std::string &error);

/*!
  Clear an event and set the header priority
*/
void
initEvent(vscpEventEx &ex, uint8_t priority);

/*!
  Data bytes as "00 01 02"
*/
std::string
dataHex(const vscpEventEx &ex);

} // namespace vscpcli

#endif
