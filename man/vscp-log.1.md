% VSCP-LOG(1) VSCP command line tools
% Ake Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-log - assemble readable VSCP node log messages

# SYNOPSIS

**vscp-log** [*options*]

# DESCRIPTION

Collect native CLASS1.LOG (509) events. Type 1 contains channel ID, level,
zero-based fragment index and up to five message bytes. Assemble separately
by full source GUID and channel, requiring consecutive indices and an
unchanged level. A new index zero starts a new message; log indices do not wrap.

Print a message after 500 ms of per-message inactivity, at the next index zero,
or on a channel start/stop event. There is no standard per-message end marker.
Inactivity is a framing heuristic and cannot detect lost final frames.
Short frames, newline and NUL do not terminate a message.
Start reception before the first fragment.

Output contains local display time, source GUID, channel, numeric/readable
level, byte count and quoted text. Levels 0-8 are Emergency, Alert, Critical,
Error, Warning, Notice, Informational, Debug and Verbose. Other values are
Application-defined. Control/non-ASCII bytes, quotes and backslashes are escaped.

General, start, stop and level events are also printed with names and
hexadecimal payload. Level-event data is preserved without assuming a
level-setting layout. Unknown types retain numeric type and raw payload.
Only native Level I log events with at most eight data bytes are collected.

# OPTIONS

**\-\-from** *GUID*
:   Exact full GUID in any supported VSCP form, including **::01**,
    **{::01,02}**, UUID or compact hex. **-** selects the zero GUID.

**-N**, **\-\-node** *NICKNAME*
:   Match the last GUID byte, 0-255, decimal or 0x-prefixed hex.
    Nicknames can repeat across interfaces. Both selectors together must
    match; without a selector all nodes are collected.

**\-\-ready-timeout** *MS*
:   Per-message inactivity boundary, default 500, minimum 1. Set longer
    than the sender's inter-fragment pauses.

**\-\-max-size** *BYTES*
:   Total buffered payload limit, default 1048576 (1 MiB), minimum 1.
    At most 1024 unfinished messages are held.

**-l**, **\-\-log** *FILE*
:   Also append displayed records to FILE.

**-q**, **\-\-quiet**
:   No stdout output.

**-n**, **\-\-count** *N*
:   Exit after N output records, including control events, not fragments.
    Zero (default) means unlimited.

**\-\-idle** *MS*, **\-\-duration** *MS*
:   Stop after no selected events for MS, or after MS total, respectively.
    Zero (default) disables each timer. Set longer than the ready timeout.

# CONNECTION AND SECURITY

All connection, configuration-file and security options are shared with
**vscp-dump**(1), including **-i**, **-c**, **-H**, **-P**, **\-\-device**,
**\-\-tls**, **\-\-encryption**, **\-\-key**, **\-\-hmac-key**,
**\-\-require-encryption**, **-v** and **\-\-print-config**.
Credentials can come from **VSCP_PASSWORD**, **VSCP_KEY**, **VSCP_HMAC_KEY**.

# EXIT STATUS

**0** normal completion or Ctrl+C; **1** malformed payload, invalid fragment
sequence/level, buffer limit or output failure; **2** usage/settings error;
**3** connection failure/loss; **4** count unmet or unfinished messages
when a stop timer expires. Unfinished data is discarded with a stderr notification.

# EXAMPLES

    vscp-log -i socketcan --device can0 -N 7
    vscp-log -i tcpip -H server --from '{::01,07}' -l node7.log
    vscp-log -i udp --ready-timeout 1000 --duration 60000 -q -l all-nodes.log

# SEE ALSO

**vscp-dump**(1), **vscp-stream**(1), **vscp-diagnostic**(1), **vscp-error**(1)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>
