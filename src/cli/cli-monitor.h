// SPDX-License-Identifier: MIT

#ifndef VSCPCLI_MONITOR_H
#define VSCPCLI_MONITOR_H

namespace vscpcli {

enum class MonitorKind { Stream, Diagnostic, Error, Log };

int runMonitor(int argc, char **argv, MonitorKind kind);

} // namespace vscpcli

#endif
