% VSCP-GEN(1) VSCP command line tools
% Åke Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-gen - generate VSCP events for testing

# SYNOPSIS

**vscp-gen** [*options*]

# DESCRIPTION

**vscp-gen** sends VSCP events with random, fixed or counting class, type,
priority, length and data, until interrupted or **\-\-count** events have been
sent. Random events never use the protocol classes CLASS1.PROTOCOL (0),
CLASS2.LEVEL1.PROTOCOL (512) and CLASS2.PROTOCOL (1024) unless
**\-\-allow-protocol** is given, as those events can change the configuration
of real nodes.

Only defined class/type pairs are generated, excluding reserved types. Fixed
class/type and size constraints must match at least one pair, or the command
fails before connecting. Level I payloads have 0-8 bytes, encapsulated Level I
payloads (classes 512-1023) have 16-24 bytes including the GUID prefix, and native
Level II payloads have 0-512 bytes. These limits apply even with **\-\-level2**.
Fixed data and fixed length must agree. Outside measurement mode, payload
semantics are not checked.

# OPTIONS

**-C**, **\-\-class** *N*|**r**
:   Class, or **r** for random (default).

**-T**, **\-\-type** *N*|**r**
:   Type, or **r** (default).

**-p**, **\-\-priority** *N*|**r**
:   Priority 0-7, or **r** (default).

**-L**, **\-\-length** *N*|**r**
:   Number of data bytes, or **r** (default).

**-D**, **\-\-data** *BYTES*|**r**|**i**
:   Fixed data, **r** random (default) or **i** an incrementing counter.

**\-\-guid** *GUID*
:   GUID of the events.

**-2**, **\-\-level2**
:   Include defined Level II classes up to 1060 (default Level I). A fixed
    class can select Level II without this flag.

**-m**, **\-\-measurements**
:   Only encoded random measurements. Select class/type with **-C**/**-T**.
    Supports classes 10, 60, 65, 70, 522, 572, 577, 582, 1040 and 1060;
    random class selection includes Level II only with **\-\-level2**.
    Values range from 0 to 100 in steps of 0.01, using the default unit
    (Kelvin for temperature), sensor/index 0, zone 0 and subzone 0.
    Encapsulated events prefix the measurement with the event GUID.
    Cannot be combined with fixed **\-\-length** or explicit **\-\-data**.

**\-\-allow-protocol**
:   Allow the protocol classes.

**-g**, **\-\-gap** *MS*
:   Gap between events (default 200).

**-b**, **\-\-burst** *N*
:   Events sent back to back each gap (default 1).

**-n**, **\-\-count** *N*
:   Stop after N events.

**\-\-seed** *N*
:   Random seed, for the same sequence each time.

**\-\-echo**
:   Print each event as it is sent.

# CONNECTION OPTIONS

These options are the same for all the VSCP command line tools.

**-i**, **\-\-interface** *TYPE*
:   **tcpip**, **udp**, **multicast**, **ws1**, **ws2**, **mqtt**,
    **socketcan** (Linux), **canal** or **level2**.

**-c**, **\-\-config** *FILE*
:   JSON file with the settings of the interface's VSCP client and **type**
    for the interface, or with these in an **interface** object together with
    stored arguments: **args** (an array of arguments for all tools) and an
    array named after a tool (for example **vscp-send**) with arguments, the
    command included, for that tool. The stored arguments come before the
    command line, so the command line overrides them; positional arguments
    are taken from the file only when the command line has none.

**\-\-set** *KEY=VALUE*
:   Set any client setting. The value is JSON if it parses as JSON, text
    otherwise. Repeatable.

**-H**, **\-\-host** *HOST*
:   Server, broker, destination address (udp) or multicast group.

**-P**, **\-\-port** *PORT*
:   Port.

**-u**, **\-\-user** *USER*
:   User name.

**\-\-password** *PASSWORD*
:   Password. Prefer the **VSCP_PASSWORD** environment variable.

**\-\-net-interface** *NAME*
:   udp, multicast: the network interface to use.

**\-\-device** *DEV*
:   socketcan: the CAN device (default vcan0).

**\-\-driver** *PATH*, **\-\-driver-config** *CFG*, **\-\-flags** *N*
:   canal and level2: the driver library and its configuration (CANAL
    configuration string or Level II configuration file), and driver flags.

**\-\-if-guid** *GUID*
:   socketcan, level2: GUID of the interface.

**\-\-connect-timeout** *MS*, **\-\-response-timeout** *MS*
:   Timeouts (default 5000 and 3000 ms).

**\-\-subscribe** *TOPIC*, **\-\-publish** *TOPIC*, **\-\-mqtt-format** *FMT*, **\-\-qos** *N*
:   mqtt: topics (default **vscp/#** and **vscp/{{guid}}/{{class}}/{{type}}**),
    publish format (json, xml, string, binary) and QoS.

**-v**, **\-\-verbose**
:   Log output on standard error, repeat for more (**-vv**).

**\-\-print-config**
:   Print the client settings as JSON (secrets masked) and exit.

# SECURITY OPTIONS

**\-\-tls**
:   Use TLS (tcpip, ws1, ws2, mqtt). The server certificate is verified.

**\-\-ca-file** *FILE*, **\-\-ca-path** *DIR*
:   CA certificates to verify the server with. MQTT over TLS needs one of
    them.

**\-\-cert** *FILE*, **\-\-cert-key** *FILE*
:   Client certificate and its private key (PEM).

**\-\-insecure**
:   Do not verify the server certificate. For testing only.

**-e**, **\-\-encryption** *ALG*
:   **none**, **aes128**, **aes192** or **aes256**. udp and multicast encrypt
    every datagram; tcpip uses the encrypted binary protocol.

**-k**, **\-\-key** *HEX*
:   AES key (32, 48 or 64 hex digits). ws1/ws2: the authentication key.
    Prefer the **VSCP_KEY** environment variable.

**\-\-hmac-key** *HEX*
:   HMAC-SHA256 key, 64 hex digits. Needed for tcpip encryption. Prefer the
    **VSCP_HMAC_KEY** environment variable.

**\-\-require-encryption**
:   udp, multicast: drop received events that are not encrypted.

**\-\-binary**
:   tcpip: use the binary protocol.

# ENVIRONMENT

**VSCP_PASSWORD**, **VSCP_KEY**, **VSCP_HMAC_KEY**
:   Password, AES key and HMAC key when not given on the command line.

# EXIT STATUS

**0** OK. **1** the operation failed. **2** command line or settings error.
**3** no connection. **4** no answer, no node found or too few events.

# EXAMPLES

Random Level I events, five a second:

    vscp-gen -i socketcan --device vcan0

10000 events as fast as possible:

    vscp-gen -i udp -g 0 -n 10000

Random temperature measurements (Kelvin):

    vscp-gen -i tcpip -H server --measurements -C 10 -T 6 -g 10

Native Level II floating point pressure measurements:

    vscp-gen -i udp --measurements -C 1060 -T 12

# SEE ALSO

**vscp-send**(1), **vscp-dump**(1), **vscp-reg**(1), **vscp-info**(1), **vscp-scan**(1), **vscpsim**(1), **vscpd**(8)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>

The VSCP specification: <https://grodansparadis.github.io/vscp-doc-spec>
