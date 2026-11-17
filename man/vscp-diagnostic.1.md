% VSCP-DIAGNOSTIC(1) VSCP command line tools
% Ake Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-diagnostic - show readable VSCP node diagnostics

# SYNOPSIS

**vscp-diagnostic** [*options*]

# DESCRIPTION

Display native CLASS1.DIAGNOSTIC (506) events with local reception time,
source GUID, numeric type and description, index/channel, zone and subzone.
Optional user information is shown in hexadecimal. Payloads must contain
3-8 bytes. Type descriptions use the pinned VSCP definitions; unknown
types are explicitly labeled and retain their numeric type and fields.

# OPTIONS

**\-\-from** *GUID*
:   Exact full GUID. All supported VSCP GUID forms are accepted, including
    **::01**, **{::01,02}**, UUID and compact hex. **-** selects the zero GUID.

**-N**, **\-\-node** *NICKNAME*
:   Match the last GUID byte, 0-255, decimal or 0x-prefixed hex.
    Nicknames can repeat across interfaces. Both selectors together must
    match; without a selector all nodes are collected.

**-l**, **\-\-log** *FILE*
:   Also append displayed records to FILE.

**-q**, **\-\-quiet**
:   No stdout output.

**-n**, **\-\-count** *N*
:   Exit after N displayed events. Zero (default) means unlimited.

**\-\-idle** *MS*, **\-\-duration** *MS*
:   Stop after no selected events for MS, or after MS total, respectively.
    Zero (default) disables each timer.

# CONNECTION AND SECURITY

All connection, configuration-file and security options are shared with
**vscp-dump**(1), including **-i**, **-c**, **-H**, **-P**, **\-\-device**,
**\-\-tls**, **\-\-encryption**, **\-\-key**, **\-\-hmac-key**,
**\-\-require-encryption**, **-v** and **\-\-print-config**.
Credentials can come from **VSCP_PASSWORD**, **VSCP_KEY**, **VSCP_HMAC_KEY**.

# EXIT STATUS

**0** normal completion or Ctrl+C; **1** malformed payload or output failure;
**2** usage/settings error; **3** connection failure/loss;
**4** count unmet when a stop timer expires.
Receiving a diagnostic event does not itself produce a failure status.

# EXAMPLES

    vscp-diagnostic -i socketcan --device can0 -N 7
    vscp-diagnostic -i tcpip -H server --from '{::01,07}' -l diagnostics.log
    vscp-diagnostic -i udp -N 7 -n 1 --duration 10000

# SEE ALSO

**vscp-dump**(1), **vscp-error**(1), **vscp-log**(1), **vscp-stream**(1)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>
