// SPDX-License-Identifier: MIT

#include <cli-monitor.h>

int
main(int argc, char **argv)
{
  return vscpcli::runMonitor(argc, argv, vscpcli::MonitorKind::Stream);
}
