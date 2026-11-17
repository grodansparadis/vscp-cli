% VSCP-STREAM(1) VSCP command line tools
% Ake Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-stream - collect VSCP Stream Data as text or hexadecimal bytes

# SYNOPSIS

**vscp-stream** [*options*]

# DESCRIPTION

Collect native CLASS1.INFORMATION (20), type 36 (0x24), Stream Data.
Byte 0 is a sequence counter, followed by up to seven payload bytes.
Each stream starts at zero; counters wrap from 255 to zero. A zero before
the expected wrap starts a new stream and displays the previous one.
Different source GUIDs are assembled separately.

There is no standard end marker. Data is displayed after 500 ms of
per-source inactivity, configurable with **\-\-ready-timeout**. Short frames,
newline and NUL do not terminate a stream. Inactivity is a framing heuristic;
lost final frames cannot be detected. Start reception before the stream begins.

Each record contains local display time, source GUID, byte count and
quoted text or hexadecimal payload. Text escapes control/non-ASCII bytes,
quotes and backslashes. Sequence bytes are excluded.
Encapsulated class 532 and zoned stream type 38 are not collected.

# OPTIONS

**-f**, **\-\-format** *FMT*
:   **text** (default), **hex** or **binary**. Binary means hexadecimal
    bytes, not raw binary stdout.

**\-\-from** *GUID*
:   Exact full GUID, in any supported VSCP form, including **::01**,
    **{::01,02}**, UUID or compact hex. **-** selects the zero GUID.

**-N**, **\-\-node** *NICKNAME*
:   Match only the last GUID byte, 0-255, decimal or 0x-prefixed hex.
    Nicknames are not globally unique. Both selectors together must match;
    without a selector all nodes are collected.

**\-\-ready-timeout** *MS*
:   Per-stream idle boundary, default 500, minimum 1.

**\-\-max-size** *BYTES*
:   Total buffered payload limit, default 1048576 (1 MiB), minimum 1.
    At most 1024 unfinished streams are held.

**-l**, **\-\-log** *FILE*
:   Also append displayed records to FILE.

**-q**, **\-\-quiet**
:   No stdout output.

**-n**, **\-\-count** *N*
:   Exit after N assembled streams, not fragments. Zero means unlimited.

**\-\-idle** *MS*, **\-\-duration** *MS*
:   Stop after no selected events for MS, or after MS total, respectively.
    Zero (default) disables each timer. Use values longer than the ready timeout.

# CONNECTION AND SECURITY

All connection, configuration-file and security options are shared with
**vscp-dump**(1), including **-i**, **-c**, **-H**, **-P**, **\-\-device**,
**\-\-tls**, **\-\-encryption**, **\-\-key**, **\-\-hmac-key**,
**\-\-require-encryption**, **-v** and **\-\-print-config**.
The shared **\-\-binary** flag selects the TCP/IP wire protocol, not output format.
Credentials can come from **VSCP_PASSWORD**, **VSCP_KEY**, **VSCP_HMAC_KEY**.

# EXIT STATUS

**0** normal completion or Ctrl+C; **1** malformed payload, invalid fragment
sequence, buffer limit or output failure; **2** usage/settings error;
**3** connection failure/loss; **4** count unmet or unfinished data when
a stop timer expires. Incomplete data is discarded with a stderr notification.

# EXAMPLES

    vscp-stream -i socketcan --device can0 -N 7 -f hex
    vscp-stream -i tcpip -H server --from '{::01,07}' --ready-timeout 1000
    vscp-stream -i udp --from -:07 -n 1 --duration 10000 -l stream.log

# SEE ALSO

**vscp-dump**(1), **vscp-log**(1), **vscp-diagnostic**(1), **vscp-error**(1)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>
