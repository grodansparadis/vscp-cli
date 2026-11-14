// cli-event.cpp
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include "cli-event.h"
#include "cli-util.h"

#include <cstring>

#include <nlohmann/json.hpp>

#include <guid.h>
#include <vscphelper.h>

namespace vscpcli {

bool
parseOutputFormat(const std::string &str, OutputFormat &fmt)
{
  std::string s = lower(str);
  if ("text" == s)
    fmt = OutputFormat::FMT_TEXT;
  else if ("string" == s)
    fmt = OutputFormat::FMT_STRING;
  else if ("json" == s)
    fmt = OutputFormat::FMT_JSON;
  else if ("xml" == s)
    fmt = OutputFormat::FMT_XML;
  else if ("csv" == s)
    fmt = OutputFormat::FMT_CSV;
  else
    return false;
  return true;
}

bool
parseTimeMode(const std::string &str, TimeMode &mode)
{
  std::string s = lower(str);
  if (("a" == s) || ("absolute" == s))
    mode = TimeMode::TIME_ABSOLUTE;
  else if (("d" == s) || ("delta" == s))
    mode = TimeMode::TIME_DELTA;
  else if (("z" == s) || ("zero" == s))
    mode = TimeMode::TIME_ZERO;
  else if (("n" == s) || ("none" == s))
    mode = TimeMode::TIME_NONE;
  else
    return false;
  return true;
}

std::string
dataHex(const vscpEventEx &ex)
{
  std::string s;
  char buf[4];
  for (int i = 0; i < ex.sizeData; i++) {
    snprintf(buf, sizeof(buf), "%02X", ex.data[i]);
    if (i) {
      s += ' ';
    }
    s += buf;
  }
  return s;
}

void
initEvent(vscpEventEx &ex, uint8_t priority)
{
  memset(&ex, 0, sizeof(ex));
  ex.head = (uint16_t) ((priority & 7) << 5);
}

std::string
EventFormatter::header(void) const
{
  if (OutputFormat::FMT_CSV == m_fmt) {
    return std::string(TimeMode::TIME_NONE == m_time ? "" : "time,") + "head,class,type,obid,guid,size,data";
  }
  return "";
}

std::string
EventFormatter::timeField(void)
{
  uint64_t now = msNow();
  if (!m_first) {
    m_first = now;
    m_last  = now;
  }
  char buf[40];
  std::string s;
  switch (m_time) {
    case TimeMode::TIME_ABSOLUTE:
      s = localTimeString();
      break;
    case TimeMode::TIME_DELTA:
      snprintf(buf, sizeof(buf), "%10.3f", (now - m_last) / 1000.0);
      s = buf;
      break;
    case TimeMode::TIME_ZERO:
      snprintf(buf, sizeof(buf), "%10.3f", (now - m_first) / 1000.0);
      s = buf;
      break;
    case TimeMode::TIME_NONE:
      break;
  }
  m_last = now;
  return s;
}

std::string
EventFormatter::format(const vscpEventEx &evIn)
{
  vscpEventEx ex = evIn; // Some library helpers take non-const
  std::string out;
  std::string t = timeField();

  switch (m_fmt) {

    case OutputFormat::FMT_TEXT: {
      cguid guid;
      guid.getFromArray(ex.GUID);
      char buf[128];
      snprintf(buf, sizeof(buf), "class=%-4u type=%-3u prio=%u ", ex.vscp_class, ex.vscp_type,
               (unsigned) ((ex.head & VSCP_HEADER_PRIORITY_MASK) >> 5));
      if (!t.empty()) {
        out = t + "  ";
      }
      out += buf;
      out += guid.getAsString();
      snprintf(buf, sizeof(buf), "  [%u]", ex.sizeData);
      out += buf;
      if (ex.sizeData) {
        out += " " + dataHex(ex);
      }
      double value;
      if (vscp_isMeasurementEx(&ex) && vscp_getMeasurementAsDoubleEx(&value, &ex)) {
        snprintf(buf, sizeof(buf), "  (measurement %g, unit %d, sensor %d)", value, vscp_getMeasurementUnitEx(&ex),
                 vscp_getMeasurementSensorIndexEx(&ex));
        out += buf;
      }
      break;
    }

    case OutputFormat::FMT_STRING:
      vscp_convertEventExToString(out, &ex);
      break;

    case OutputFormat::FMT_JSON: {
      std::string str;
      vscp_convertEventExToJSON(str, &ex);
      try {
        out = nlohmann::json::parse(str).dump(); // One line
      }
      catch (...) {
        out = str;
      }
      break;
    }

    case OutputFormat::FMT_XML:
      vscp_convertEventExToXML(out, &ex);
      for (auto &c : out) {
        if (('\n' == c) || ('\r' == c)) {
          c = ' ';
        }
      }
      break;

    case OutputFormat::FMT_CSV: {
      cguid guid;
      guid.getFromArray(ex.GUID);
      char buf[64];
      if (TimeMode::TIME_NONE != m_time) {
        out = trim(t) + ",";
      }
      snprintf(buf, sizeof(buf), "%u,%u,%u,%u,", ex.head, ex.vscp_class, ex.vscp_type, ex.obid);
      out += buf;
      out += guid.getAsString();
      out += "," + std::to_string(ex.sizeData) + "," + dataHex(ex);
      break;
    }
  }

  return out;
}

bool
parseEventLine(const std::string &lineIn, vscpEventEx &ex, std::string &error)
{
  std::string line = trim(lineIn);
  memset(&ex, 0, sizeof(ex));

  if (line.empty()) {
    error = "empty event";
    return false;
  }

  bool ok;
  if ('{' == line[0]) {
    ok = vscp_convertJSONToEventEx(&ex, line);
  }
  else if ('<' == line[0]) {
    ok = vscp_convertXMLToEventEx(&ex, line);
  }
  else {
    ok = vscp_convertStringToEventEx(&ex, line);
  }

  if (!ok) {
    error = "'" + line + "' is not a valid event (VSCP string, JSON or XML)";
  }
  return ok;
}

} // namespace vscpcli
