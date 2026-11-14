% VSCP-SEND(1) VSCP command line tools
% Åke Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-send - send VSCP events

# SYNOPSIS

**vscp-send** [*options*] *CLASS* *TYPE* [*DATA* ...]

**vscp-send** [*options*] **-E** *EVENT* [**-E** *EVENT* ...]

**vscp-send** [*options*] **\-\-file** *FILE*|**-**

# DESCRIPTION

**vscp-send** sends VSCP events over any VSCP interface. *CLASS*, *TYPE* and
the data bytes are decimal or hex (0x..). Data can also be one comma
separated list (1,2,0x03) or hex pairs after a colon (:010203).

Events can also be given in the VSCP string format
(head,class,type,obid,datetime,timestamp,GUID,data...), as JSON or as XML,
on the command line or from a file. **vscp-dump -f string** and **-f json**
write files that can be sent again.

# OPTIONS

**-g**, **\-\-guid** *GUID*
:   GUID of the event. Default all zero, the interface or server uses its own.

**-p**, **\-\-priority** *0-7*
:   Priority, 0 is the highest (default 3).

**-E**, **\-\-event** *EVENT*
:   An event as VSCP string, JSON or XML. Repeatable.

**-f**, **\-\-file** *FILE*
:   Events from a file, one per line (**-** is standard input). Lines
    starting with # are skipped.

**\-\-no-check**
:   Allow more than 8 data bytes in Level I classes (below 512).

**-n**, **\-\-count** *N*
:   Send everything N times. 0 sends until interrupted.

**-d**, **\-\-delay** *MS*
:   Delay between events.

**\-\-linger** *MS*
:   Wait before disconnecting (default 500 for mqtt).

**\-\-echo**
:   Print each event as it is sent.

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

CLASS1.INFORMATION ON, index 0, zone 1, subzone 2, over UDP:

    vscp-send -i udp 20 3 0 1 2

23 °C (CLASS1.MEASUREMENT temperature) on a CAN bus at the highest priority:

    vscp-send -i socketcan --device can0 -p 0 10 6 0x89 0x81 0x00 0xe6

To a VSCP daemon with TLS and AES-256 + HMAC, keys in the environment:

    vscp-send -i tcpip -H server --tls --ca-file ca.crt -e aes256 20 3 0 1 2

Replay a recording:

    vscp-send -i tcpip -H server --file events.txt -d 10

# SEE ALSO

**vscp-dump**(1), **vscp-gen**(1), **vscp-reg**(1), **vscp-info**(1), **vscp-scan**(1), **vscpsim**(1), **vscpd**(8)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>

The VSCP specification: <https://grodansparadis.github.io/vscp-doc-spec>
