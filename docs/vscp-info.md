# vscp-info

Show information about a VSCP node from its standard registers: GUID, MDF
URL, firmware version, manufacturer ids, bootloader and more. `--mdf`
downloads the node's MDF (Module Description File) and shows what it
describes.

```
vscp-info [OPTIONS] -N NODE
```

## Options

| Option | Description |
| :----- | :---------- |
| `-N, --node NICKNAME` | The node: its nickname (0-255) or its GUID |
| `--via GUID` | GUID of the VSCP server interface the node is on |
| `--timeout MS` | Timeout for the register reads (default 2000) |
| `--guid` | Only print the GUID |
| `--mdf-url` | Only print the MDF URL |
| `-m, --mdf` | Download the MDF and show module, manufacturer and registers |
| `--mdf-save FILE` | Download the MDF to FILE |
| `--mdf-from URL` | Use this MDF URL instead of the one in the node |
| `-j, --json` | JSON output |

Plus the [connection options](connecting.md).

The MDF URL in a node has room for 32 characters and is normally stored
without `http://`; vscp-info adds it. The MDF is fetched over http or https
only.

## Example

```
$ vscp-info -i socketcan --device can0 -N 42 --mdf
Nickname           42 (0x2A)
GUID               FF:FF:FF:FF:FF:FF:FF:FE:00:00:00:00:00:00:00:2A
MDF URL            http://www.grodansparadis.com/paris/paris.xml
Firmware           1.1.2
Manufacturer id    0x00000000 / sub 0x00000000
Bootloader         VSCP (0x00)
Buffer size        8
Register pages     1 (selected page 0)
...

MDF
  Module           paris relay controller
  Model            B
  Version          1.1.2 (2020-05-15)
  Manufacturer     Paradise of the Frog AB
  Registers        118
    0000:00  rw  zone
    0000:01  rw  subzone
    ...
```

```bash
# In a script
guid=$(vscp-info -i udp -N 7 --guid)

# Everything as JSON, keep the MDF
vscp-info -i tcpip -H server -N 7 --mdf-save node7.xml -j > node7.json
```

## Exit status

0 OK, 1 the MDF could not be fetched or parsed, 2 command line error, 3 no
connection, 4 the node did not answer.

---

[filename](./bottom_copyright.md ':include')