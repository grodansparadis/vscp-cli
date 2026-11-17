% VSCP-SCAN(1) VSCP command line tools
% Åke Hedman, Grodans Paradis AB
% November 2026

# NAME

vscp-scan - find the VSCP nodes on a segment

# SYNOPSIS

**vscp-scan** [*options*]

# DESCRIPTION

**vscp-scan** finds the VSCP nodes on a segment. The default fast scan sends
one "who is there" event that all nodes answer with their GUID and MDF URL.
**\-\-slow** reads a register of every nickname in a range instead and also
finds nodes that do not answer "who is there".

The VSCP firmware sends the GUID in the "who is there" answer least
significant byte first and **vscp-scan** decodes it that way. With **\-\-info**
the GUID is read from the node's registers instead.

GUID arguments accept colon-separated, UUID and compact VSCP forms such as
**::01** (leading FF bytes) and **-:01** (leading zeroes).

# OPTIONS

**-s**, **\-\-slow**
:   Register read scan of the nicknames **\-\-from** to **\-\-to**.

**\-\-from** *N*, **\-\-to** *N*
:   Nickname range for **\-\-slow** (default 0-254).

**\-\-timeout** *MS*
:   How long to wait for answers (default 2000, 3000 with **\-\-slow**).

**\-\-delay** *MS*
:   **\-\-slow**: delay between the read requests (default 5).

**\-\-via** *GUID*
:   GUID of the server interface to scan behind.

**\-\-info**
:   Also read the standard registers of each node found.

**-j**, **\-\-json**
:   JSON output.

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

    vscp-scan -i socketcan --device can0

    vscp-scan -i udp --info

    vscp-scan -i tcpip -H server --slow --from 1 --to 32 -j

# SEE ALSO

**vscp-send**(1), **vscp-dump**(1), **vscp-gen**(1), **vscp-reg**(1), **vscp-info**(1), **vscpsim**(1), **vscpd**(8)

The full documentation: <https://github.com/grodansparadis/vscp-cli/tree/main/docs>

The VSCP specification: <https://grodansparadis.github.io/vscp-doc-spec>
