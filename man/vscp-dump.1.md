% VSCP-DUMP(1) VSCP command line tools
% Åke Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-dump - show received VSCP events

# SYNOPSIS

**vscp-dump** [*options*]

# DESCRIPTION

**vscp-dump** prints the VSCP events received on any VSCP interface, one per
line, until interrupted. The output can be text, VSCP string, JSON, XML or CSV;
string and JSON lines can be sent again with **vscp-send \-\-file**.

# OPTIONS

**-f**, **\-\-format** *FMT*
:   **text** (default), **string**, **json**, **xml** or **csv**.

**-t**, **\-\-time** *MODE*
:   Time stamp in text and csv output: **a** absolute local time (default),
    **d** since the previous event, **z** since the first event, **n** none.

**-l**, **\-\-log** *FILE*
:   Also append the output to FILE.

**-q**, **\-\-quiet**
:   No output on standard output.

**-C**, **\-\-class** *LIST*
:   Only these classes, e.g. 10,20. Repeatable.

**-T**, **\-\-type** *LIST*
:   Only these types. Repeatable.

**\-\-from** *GUID*
:   Only events from this GUID.

**\-\-filter** *FILTER*, **\-\-mask** *MASK*
:   VSCP filter and mask, each priority,class,type,GUID.

**-n**, **\-\-count** *N*
:   Exit after N events.

**\-\-idle** *MS*
:   Exit when no event has arrived for MS milliseconds.

**\-\-duration** *MS*
:   Exit after MS milliseconds.

# CONNECTION OPTIONS

These options are the same for all the VSCP command line tools.

**-i**, **\-\-interface** *TYPE*
:   **tcpip**, **udp**, **multicast**, **ws1**, **ws2**, **mqtt**,
    **socketcan** (Linux), **canal** or **level2**.

**-c**, **\-\-config** *FILE*
:   JSON file with the settings of the interface's VSCP client and **type**
    for the interface. Command line options override it.

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

Everything on a CAN bus with the time between events:

    vscp-dump -i socketcan --device can0 -t d

Measurements from a VSCP daemon as JSON lines:

    vscp-dump -i tcpip -H server -C 10,15,1040 -f json

Encrypted UDP, plain datagrams are dropped:

    vscp-dump -i udp -e aes128 --require-encryption

Wait at most 10 s for a heartbeat (status 4 if none came):

    vscp-dump -i udp -C 20 -T 9 -n 1 --duration 10000 -q

# SEE ALSO

**vscp-send**(1), **vscp-gen**(1), **vscp-reg**(1), **vscp-info**(1), **vscp-scan**(1), **vscpsim**(1), **vscpd**(8)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>

The VSCP specification: <https://grodansparadis.github.io/vscp-doc-spec>
