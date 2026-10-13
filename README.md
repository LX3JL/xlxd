# XLX Multiprotocol Reflector Server

## Copyright

(c) 2016 Jean-Luc Deltombe LX3JL and Luc Engelmann LX1IQ

The XLX Multiprotocol Gateway Reflector Server is part of the software system
for the D-Star Network.
The sources are published under GPL Licenses.

---

## Supported Protocols (XLX v2.6.x with M17 Extension, YSF Peer, NXDN Peer, P25 Peer, REF Peer, XRF Peer and DCS Peer)

- **D-Star**: Icom-G3Terminal, DExtra, DPlus and DCS
- **DMR**: DMRPlus (dongle) and DMRMmdvm
- **C4FM**: YSF, YSF Client, Wires-X and IMRS
- **NXDN**: NXDN Reflector Peering (peer-only, same AMBE2+ codec as DMR - no transcoding needed)
- **P25**: P25 Reflector Peering (peer-only, IMBE codec - requires AMBEd software transcoding)
- **REF**: REF/DPlus Reflector Peering (peer-only, same AMBE+ codec as D-Star - no transcoding needed)
- **XRF**: XRF/DExtra Reflector Peering (peer-only, same AMBE+ codec as D-Star - no transcoding needed)
- **DCS**: DCS Reflector Peering (peer-only, same AMBE+ codec as D-Star - no transcoding needed)
- **M17**: Full M17 protocol support with Codec2 software transcoding
- **XLX Interlink**: Protocol for linking reflectors (revision 3 with M17 Codec2 support)

---

## XLX Interlink Protocol

XLX reflectors can peer with each other to share voice traffic. The interlink protocol has evolved to support multiple codecs:

### Protocol Revisions

| Revision | XLX Version | Packet Size | Codecs Included |
|----------|-------------|-------------|-----------------|
| 0/1 | < 2.2.0 | 27 bytes | D-Star AMBE only |
| 2 | 2.2.0+ | 45 bytes | D-Star AMBE + DMR AMBE+ |
| **3** | **2.6.0+** | **53 bytes** | **D-Star AMBE + DMR AMBE+ + M17 Codec2** |

### How Protocol Negotiation Works

When two XLX reflectors connect, they exchange version numbers. The sending reflector checks the receiver's version and sends the appropriate packet size:

- **Receiver < 2.2.0**: Receives 27-byte packets (D-Star only)
- **Receiver 2.2.0 - 2.5.x**: Receives 45-byte packets (D-Star + DMR)
- **Receiver 2.6.0+**: Receives 53-byte packets (D-Star + DMR + M17)

### Benefits of Protocol Revision 3

When two XLX reflectors running v2.6.0+ are peered:

1. **No M17 Transcoding Required**: M17 Codec2 audio passes directly between reflectors
2. **Reduced CPU Load**: Eliminates encode/decode cycles for M17 traffic
3. **Lower Latency**: Direct codec passthrough instead of transcoding chain
4. **Full Backward Compatibility**: Older reflectors still work (receive appropriate subset)

### Packet Structure

```
Revision 3 DV Frame Packet (53 bytes):
┌───────────────────────────────────────────────────────────────────────────┐
│ DSVT Header (12) │ StreamID (2)  │ PacketID (1) │ AMBE (9)   │ DVData (3) │
├───────────────────────────────────────────────────────────────────────────┤
│ DMR_PktID (1)    │ DMR_SubID (1) │ AMBE+ (9)    │ DVSync (7) │ Codec2 (8) │
└───────────────────────────────────────────────────────────────────────────┘
         └── Rev 0/1: 27 bytes ──┘                          │
         └────────── Rev 2: 45 bytes ───────────────────────┘
         └────────────── Rev 3: 53 bytes ───────────────────────────────┘
```

### Example: Two v2.6.0+ Reflectors Linked

```
XLX001 (v2.6.0)                              XLX002 (v2.6.0)
      │                                            │
      │◀── Protocol Rev 3 (53-byte packets) ──────▶│
      │                                            │
  ┌───────┐                                    ┌───────┐
  │  M17  │ ◀─── Codec2 passthrough ──────────▶│  M17  │
  │ Client│     (no transcoding needed)        │ Client│
  └───────┘                                    └───────┘
```

---

## Important: AMBEd Update Required for XLX v2.6.0+

**If you are upgrading to XLX v2.6.0 or later, you MUST also update your AMBEd (AMBE transcoder daemon).**

### Version Compatibility

| XLX Version | AMBEd Version | Transcoding Support |
|-------------|---------------|---------------------|
| v2.6.0+ | v1.4.0+ | Full support (D-Star/DMR/YSF ↔ M17 ↔ P25) |
| v2.6.0+ | v1.3.x | D-Star ↔ DMR only (no M17/P25) |
| v2.5.x | v1.3.x | D-Star ↔ DMR only |

**AMBEd v1.4.0** includes:
- Codec2 software vocoder for M17 transcoding (encode and decode)
- IMBE software vocoder for P25 transcoding (encode and decode)
- Multi-codec response protocol supporting up to 3 output codecs per stream
- Dynamic max concurrent streams calculated from available hardware channels
- DSCP/QoS marking for voice packets (configurable, default EF/46)
- Byte-order fix for DVSI hardware PCM output

### Upgrade Steps

1. Update XLX:
   ```bash
   cd xlxd
   git pull
   cd src && make clean && make && make install
   ```

2. Update AMBEd:
   ```bash
   cd xlxd/ambed
   make clean && make && make install
   ```

3. Restart both services:
   ```bash
   systemctl restart ambed
   systemctl restart xlxd
   ```

### Current Transcoding Support (v2.6.0 + AMBEd v1.4.0)

| Path | Status | Notes |
|------|--------|-------|
| D-Star ↔ DMR | Working | Hardware transcoding via AMBEd |
| D-Star ↔ YSF | Working | Hardware transcoding via AMBEd |
| DMR ↔ YSF | Working | Same codec (AMBE2+), no transcoding needed |
| M17 ↔ M17 | Working | Native Codec2, no transcoding needed |
| M17 → D-Star/DMR/YSF | Working | Software Codec2 decode + hardware AMBE encode |
| D-Star/DMR/YSF → M17 | Working | Hardware AMBE decode + software Codec2 encode (AMBEd v1.4.0+) |
| P25 → D-Star | Working | Software IMBE decode + hardware AMBE+ encode |
| P25 → DMR/YSF/NXDN | Working | Software IMBE decode + hardware AMBE2+ encode |
| P25 → M17 | Working | Software IMBE decode + software Codec2 encode |
| D-Star/DMR/YSF → P25 | Working | Hardware AMBE decode + software IMBE encode |
| M17 → P25 | Working | Software Codec2 decode + software IMBE encode |

### DMR/YSF/NXDN Audio Level Balancing

DMR, YSF, and NXDN all use the same AMBE+2 codec but with different audio level conventions. XLX automatically adjusts gains when bridging between these modes to provide consistent audio levels.

**Current gain adjustments** (configured in `main.h`):

| Path | Gain | Notes |
|------|------|-------|
| YSF → DMR | +10 dB | YSF audio is quieter than DMR convention |
| DMR → YSF | -4 dB | Reduce DMR levels for YSF |
| NXDN → DMR | -3 dB | Reduce NXDN levels for DMR |
| DMR → NXDN | -3 dB | Reduce DMR levels for NXDN |

This adjustment is performed in the AMBE+2 bitstream by modifying the b2 voice parameter, which requires:
1. Golay FEC decoding to extract voice parameters
2. Modification of the 5-bit b2 gain index
3. PRNG re-scrambling with the updated parameters
4. Golay FEC re-encoding

The adjustment is applied once at the reflector level, providing consistent audio levels for all users without requiring individual radio configuration changes.

---

## M17 Protocol Support

This version of XLX includes full M17 digital voice protocol support with bidirectional transcoding to all other supported modes.

### Key Features

- **Full M17 Protocol Implementation**: Native M17 reflector connectivity on UDP port 17000
- **Hybrid Transcoding**: M17↔DMR/YSF uses software only; M17↔D-Star uses software + hardware chain
- **Bidirectional Audio**: M17 users hear all other users (DMR/D-Star/YSF), and vice versa
- **Dual Identity**: Reflector responds to both XLX-xxx and M17-xxx callsigns
- **Module Support**: Full A-Z module mapping
- **Quality Optimized**: Uses AMBE2+ as intermediate codec for better audio quality

### How M17 Transcoding Works

M17 transcoding uses DVSI hardware via AMBEd for all paths:

- **M17 → D-Star/DMR/YSF**: Codec2 decode (software in ambed) → PCM → AMBE encode (hardware)
- **D-Star/DMR/YSF → M17**: AMBE decode (hardware) → PCM → Codec2 encode (software in ambed)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    M17 ↔ DMR/D-Star Transcoding (via AMBEd)                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   M17 Client                    AMBEd                      DMR/D-Star       │
│   (Codec2)                   (transcoder)                   (AMBE)          │
│        │                          │                           │             │
│        ▼                          ▼                           ▼             │
│   ┌─────────┐              ┌─────────────┐              ┌─────────┐         │
│   │ Codec2  │─────────────▶│   Codec2    │              │  AMBE   │         │
│   │  Frame  │              │   Decode    │─────────────▶│  Frame  │         │
│   │(8 bytes)│              │  (software) │   DVSI HW    │(9 bytes)│         │
│   └─────────┘              │      ↓      │   Encode     └─────────┘         │
│                            │     PCM     │                                  │
│   ┌─────────┐              │      ↓      │              ┌─────────┐         │
│   │ Codec2  │◀─────────────│   Codec2    │              │  AMBE   │         │
│   │  Frame  │              │   Encode    │◀─────────────│  Frame  │         │
│   │(8 bytes)│              │  (software) │   DVSI HW    │(9 bytes)│         │
│   └─────────┘              └─────────────┘   Decode     └─────────┘         │
│                                                                             │
│                        USES DVSI HARDWARE CHANNELS                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Transcoding Components

| Component | Purpose | Location |
|-----------|---------|----------|
| **Codec2** | M17 voice codec (3200bps) | Software in xlxd (decode) and ambed (encode) |
| **DVSI Hardware** | AMBE+/AMBE2+ encode/decode | Hardware chips via ambed |

### M17 Protocol Details

- **Port**: UDP 17000
- **Keepalive**: PING/PONG every 5 seconds
- **Timeout**: 35 seconds without PONG
- **Frame Size**: 54 bytes (including "M17 " magic)
- **Voice Codec**: Codec2 3200bps (8 bytes per 20ms frame)
- **Callsign Encoding**: Base-40 (9 characters in 6 bytes)

### Configuration (main.h)

```cpp
// M17 Protocol Settings
#define M17_PORT                17000   // UDP port
#define M17_KEEPALIVE_PERIOD    5       // seconds
#define M17_KEEPALIVE_TIMEOUT   35      // seconds

// Codec identifier
#define CODEC_CODEC2            3       // M17 uses Codec2
```

### Transcoder Channel Usage

M17 transcoding uses software Codec2 plus DVSI hardware for AMBE:

| Path | Method | DVSI Channels |
|------|--------|---------------|
| M17 ↔ DMR/YSF | Software Codec2 + Hardware AMBE2+ | 1 per stream |
| M17 ↔ D-Star | Software Codec2 + Hardware AMBE+ | 1 per stream |
| DMR ↔ D-Star | Hardware AMBE2+ ↔ AMBE+ | 2 per stream |
| DMR ↔ YSF | Same codec (AMBE2+) | 0 |

Example with 6 DVSI channels:
- DMR ↔ D-Star: Uses 2 hardware channels per simultaneous stream
- M17 ↔ any: Uses 1 hardware channel per stream (Codec2 is software)

---

## General Usage

The packages which are described in this document are designed to install server
software which is used for the D-Star network infrastructure.
It requires a 24/7 internet connection which can support 20 voice streams or more
to connect repeaters and hotspot dongles.

- The server requires a fixed IP-address
- The public IP address should have a DNS record which must be published in the
common host files

If you want to run this software please make sure that you can provide this
service free of charge, like the developer team provides the software and the
network infrastructure free of charge!

---

## Requirements

The software packages for Linux are tested on Debian 7 (Wheezy) 32 and 64bit or newer.
Raspbian will work but is not recommended.
Please use the stable version listed above, we cannot support others.

---

## Installation

### Debian/Ubuntu

#### After a clean installation, run update and upgrade
```bash
apt-get update
apt-get upgrade
```

#### Install required packages
```bash
apt-get install git git-core
apt-get install apache2 php
apt-get install build-essential
```

#### Download and compile the XLX sources
```bash
git clone https://github.com/LX3JL/xlxd.git
cd xlxd/src/
```

#### Configure main.h before compiling
```bash
nano main.h
```

Important settings to check:
- `NB_OF_MODULES` - Number of modules (default 10, max 26)
- `YSF_DEFAULT_NODE_TX_FREQ` / `YSF_DEFAULT_NODE_RX_FREQ` - YSF frequencies
- M17 clients must specify a valid module (A-Z)

#### Compile and install
```bash
make clean
make
make install
```

#### Setup startup script
```bash
cp ~/xlxd/scripts/xlxd /etc/init.d/xlxd
nano /etc/init.d/xlxd  # Adapt parameters to your needs
update-rc.d xlxd defaults
```

#### Download DMR ID database
```bash
wget -O /xlxd/dmrid.dat http://xlxapi.rlx.lu/api/exportdmr.php
```

#### Setup AMBE transcoder (if using hardware)
Follow the readme in the AMBEd directory for FTDI driver and AMBE service setup.

#### Start/Stop the service
```bash
service xlxd start
service xlxd stop
```

#### Install dashboard
```bash
cp -r ~/xlxd/dashboard /var/www/db
chmod +r /var/log/messages
```

#### Reboot to test auto-start
```bash
reboot
```

---

## Firewall Settings

XLX Server requires the following ports to be open and forwarded properly:

| Port | Protocol | Service |
|------|----------|---------|
| 80 | TCP | HTTP (dashboard) |
| 443 | TCP | HTTPS (optional) |
| 8080 | TCP | RepNet (optional) |
| 10001 | UDP | JSON interface |
| 10002 | UDP | XLX interlink |
| 22 | TCP | SSH |
| 17000 | UDP | M17 protocol |
| 42000 | UDP | YSF protocol |
| 30001 | UDP | DExtra protocol |
| 20001 | UDP | DPlus protocol |
| 30051 | UDP | DCS protocol |
| 8880 | UDP | DMR+ DMO mode |
| 62030 | UDP | MMDVM protocol |
| 10100 | UDP | AMBE controller |
| 10101-10199 | UDP | AMBE transcoding |
| 12345-12346 | UDP | Icom Terminal |
| 40000 | UDP | Icom Terminal DV |
| 21110 | UDP | Yaesu IMRS |

---

## M17 Client Connection

M17 clients can connect using:

- **Reflector callsign**: `M17-XXX` or `XLX-XXX` (where XXX is your reflector ID)
- **Module**: A-Z (must be specified, invalid modules rejected)
- **Port**: 17000/UDP

### Connection Sequence

```
Client                              Reflector
   |                                    |
   |-------- CONN (callsign) ---------->|
   |                                    |
   |<--------- ACKN / NACK -------------|
   |                                    |
   |<---------- PING -------------------|
   |                                    |
   |----------- PONG ------------------>|
   |           (every 5s)               |
   |                                    |
   |-------- Voice Frames ------------->|
   |<------- Voice Frames --------------|
   |                                    |
   |---------- DISC ------------------->|
   |                                    |
```

---

## Architecture Overview

### Source Files for M17 Support

| File | Purpose |
|------|---------|
| `cm17protocol.h/cpp` | M17 protocol handler |
| `cm17client.h/cpp` | M17 client management |
| `codec2/*.cpp` | Embedded Codec2 library (decode only in xlxd) |

### Source Files for YSF Peer Support

| File | Purpose |
|------|---------|
| `cysfpeer.h/cpp` | YSF peer management |
| `cysfpeerclient.h/cpp` | YSF peer client for voice routing |
| `cpeercallsignlist.h/cpp` | Peer list with YSF/NXDN support |

### Source Files for NXDN Peer Support

| File | Purpose |
|------|---------|
| `cnxdnprotocol.h/cpp` | NXDN protocol handler (peer-only) |
| `cnxdnpeer.h/cpp` | NXDN peer management |
| `cnxdnpeerclient.h/cpp` | NXDN peer client for voice routing |
| `nxdndefines.h` | NXDN protocol constants |
| `cnxdniddir.h/cpp` | NXDN ID database base class |
| `cnxdniddirhttp.h/cpp` | NXDN ID database HTTP fetcher |

### Source Files for P25 Peer Support

| File | Purpose |
|------|---------|
| `cp25protocol.h/cpp` | P25 protocol handler (peer-only) |
| `cp25peer.h/cpp` | P25 peer management |
| `cp25peerclient.h/cpp` | P25 peer client for voice routing |
| `p25defines.h` | P25 protocol constants |

### Source Files for REF/DPlus Peer Support

| File | Purpose |
|------|---------|
| `cdplusprotocol.h/cpp` | DPlus protocol handler (clients + REF peers) |
| `cdpluspeer.h/cpp` | DPlus/REF peer management |
| `cdpluspeerclient.h/cpp` | DPlus peer client for voice routing |

### Source Files for XRF/DExtra Peer Support

| File | Purpose |
|------|---------|
| `cdextraprotocol.h/cpp` | DExtra protocol handler (clients + XRF peers) |
| `cdextrapeer.h/cpp` | DExtra/XRF peer management |
| `cdextrapeerclient.h/cpp` | DExtra peer client for voice routing |

### Source Files for DCS Peer Support

| File | Purpose |
|------|---------|
| `cdcsprotocol.h/cpp` | DCS protocol handler (clients + DCS peers) |
| `cdcspeer.h/cpp` | DCS peer management |
| `cdcspeerclient.h/cpp` | DCS peer client for voice routing |

### Class Hierarchy

```
CProtocol (base)
    └── CM17Protocol (M17 protocol handler)

CClient (base)
    └── CM17Client (M17 client)

CPacketQueue (base)
    └── CCodecStream (transcoder stream to AMBEd)
```

### Protocol Threading Model

Most protocol handlers use a split RX/TX thread architecture. The receive thread (RxTask) handles inbound packets, stream open/close, and timeout detection. The transmit thread (TxTask) handles outbound packet queuing, keepalives, and peer management, waking via a condition variable when packets are queued.

**Split RX/TX threads:** DExtra (XRF), DPlus (REF), DCS, M17, YSF, NXDN, P25
**Single thread (legacy):** XLX interlink, DMR+ (DMRplus), DMR MMDVM

This separation was introduced to prevent transmission tail loss — in the original single-threaded design, time spent processing outbound packets could delay reception of inbound packets, causing the final frames of a transmission to be missed or the stream to time out prematurely. With split threads, the RX path is never blocked by TX processing.

Each split protocol's stream cache uses per-slot mutexes to protect shared state between the RX and TX threads. Lock ordering is enforced across all protocols to prevent deadlocks: global locks (Peers, Clients, GateKeeper) are never held simultaneously, and peer/gatekeeper data is snapshotted into local variables before acquiring other locks.

---

## YSF Master Server

The XLX Server acts as a YSF Master, providing 26 Wires-X rooms.
It is separate from the regular YSFReflector network - registration at ysfreflector.de is optional.

---

## YSF Reflector Peering

XLX can connect directly to YSF reflectors as a peer, allowing bidirectional voice between YSF reflector users and XLX users (DMR, D-Star, M17, etc.).

### Configuration

Add YSF peers to your `xlxd.interlink` file:

```
# YSF Reflector Peering
# Format: YSF<id> <hostname>:<port> <module>
#
# - YSF ID must be 5 digits (pad with zeros if needed)
# - Port is required (typically 42000 for YSF reflectors)
# - Only ONE module allowed per YSF peer (they are single-room)

YSF12345 ysf.example.com:42000 B
YSF00999 192.168.1.100:42000 E
```

### How It Works

- XLX connects to the YSF reflector as a client using native YSF protocol
- Voice traffic flows bidirectionally between the linked module and the YSF reflector
- YSF and DMR both use AMBE2+ codec, so no transcoding is needed between them
- D-Star and M17 users are transcoded as normal

### Notes

- The YSF reflector must be accessible from your XLX server
- YSF peers appear alongside XLX peers in the dashboard
- Traffic shows the originating user's callsign, not the reflector callsign

---

## NXDN Reflector Peering

XLX can connect directly to NXDN reflectors as a peer, allowing bidirectional voice between NXDN reflector users and XLX users (DMR, D-Star, M17, YSF, etc.).

### Configuration

Add NXDN peers to your `xlxd.interlink` file:

```
# NXDN Reflector Peering
# Format: NXnnnnn <hostname>:<port> <module>
#
# - NXDN ID is 5 digits (00001-65535, pad with zeros if needed)
# - Port is REQUIRED (no default - NXDN reflectors use various ports)
# - Only ONE module allowed per NXDN peer (they are single-room)

NX31672 nxdn.example.com:41400 E
NX00100 192.168.1.100:41400 B
```

### How It Works

- XLX connects to the NXDN reflector as a client using native NXDN protocol
- Voice traffic flows bidirectionally between the linked module and the NXDN reflector
- **No AMBEd required**: NXDN uses the same AMBE2+ codec as DMR/YSF, so voice data passes directly through XLXd without transcoding
- Audio gain is automatically adjusted between NXDN↔DMR (see DMR/YSF/NXDN Audio Level Balancing section)
- D-Star and M17 users are transcoded via AMBEd as normal

### Protocol Details

| Packet Type | Size | Purpose |
|-------------|------|---------|
| NXDNP | 17 bytes | Poll/Connect/Keepalive |
| NXDNU | 17 bytes | Unlink |
| NXDND | 43 bytes | Voice/Data |

### NXDN ID Database

XLX automatically downloads and caches the NXDN user database every 12 hours. This allows XLX to display callsigns instead of numeric NXDN IDs when users transmit.

**Data Sources:**
- Primary: `http://database.radioid.net/static/nxdn.csv`
- Fallback: `http://www.pistar.uk/downloads/NXDN.csv`

The user-agent is set to `XLX {version} NXDN Client Updater` to properly identify XLX to the upstream servers.

Configuration in `main.h`:
```cpp
#define NXDNIDDB_USE_RLX_SERVER         1       // 1 = use http, 0 = use local file
#define NXDNIDDB_PATH                   "/xlxd/nxdn.csv"  // local file path
#define NXDNIDDB_REFRESH_RATE           720     // in minutes (12 hours)
```

### Notes

- NXDN IDs are 16-bit (1-65535), represented as 5-digit strings
- The NXDN reflector must be accessible from your XLX server
- NXDN peers appear alongside other peers in the dashboard
- User callsigns are looked up from the NXDN ID database (falls back to "NXDNnnnnn" if not found)

---

## P25 Reflector Peering

XLX can connect directly to P25 reflectors as a peer, allowing bidirectional voice between P25 reflector users and XLX users (DMR, D-Star, M17, YSF, NXDN, etc.).

**Note:** P25 support is peer-only. XLX does not support direct P25 client connections - users must connect via a P25 reflector that is peered with XLX.

### Configuration

Add P25 peers to your `xlxd.interlink` file:

```
# P25 Reflector Peering
# Format: P25nnnnn <hostname>:<port> <module>
#
# - P25 ID is 5 digits (pad with zeros if needed)
# - Port is REQUIRED (no default - P25 reflectors use various ports)
# - Only ONE module allowed per P25 peer (they are single-room)

P2531672 p25.example.com:41000 E
P2500100 192.168.1.100:41000 B
```

### How It Works

- XLX connects to the P25 reflector using native P25 protocol
- Voice traffic flows bidirectionally between the linked module and the P25 reflector
- **AMBEd required**: Unlike NXDN which shares the AMBE2+ codec with DMR, P25 uses the IMBE codec which must be transcoded via software vocoders in AMBEd v1.4.0+
- All other modes (D-Star, DMR, YSF, NXDN, M17) are transcoded automatically

### Transcoding Details

P25 uses the IMBE (Improved Multi-Band Excitation) codec. AMBEd v1.4.0+ includes software IMBE encode/decode:

| Path | Method |
|------|--------|
| P25 → D-Star | Software IMBE decode → PCM → Hardware AMBE+ encode |
| P25 → DMR/YSF/NXDN | Software IMBE decode → PCM → Hardware AMBE2+ encode |
| P25 → M17 | Software IMBE decode → PCM → Software Codec2 encode |
| D-Star → P25 | Hardware AMBE+ decode → PCM → Software IMBE encode |
| DMR/YSF/NXDN → P25 | Hardware AMBE2+ decode → PCM → Software IMBE encode |
| M17 → P25 | Software Codec2 decode → PCM → Software IMBE encode |

### Notes

- P25 peers appear alongside other peers in the dashboard
- The P25 reflector must be accessible from your XLX server
- P25 transcoding uses 2 DVSI hardware channels per stream (for AMBE encode/decode paths)
- Pure software paths (P25 ↔ M17) do not require DVSI hardware channels

---

## REF/DPlus Reflector Peering

XLX can connect directly to REF reflectors as a peer using the DPlus protocol, allowing bidirectional voice between REF reflector users and XLX users (DMR, M17, YSF, NXDN, P25, etc.).

**Note:** REF peering support is outbound only (XLX connects to REF as a client). REF reflectors cannot initiate connections to XLX.

### Configuration

Add REF peers to your `xlxd.interlink` file:

```
# REF/DPlus Reflector Peering
# Format: REFnnn <hostname>[:port] <local><remote>
#
# - REF callsign must be REFnnn format (e.g., REF001, REF123)
# - Port defaults to 20001 if not specified
# - Module mapping: first letter = local XLX module, second = remote REF module
# - Both module letters must be A-Z

REF001 ref001.example.com EA          # XLX module E links to REF001 module A
REF030 ref030.dstargateway.org:20001 AA    # XLX module A links to REF030 module A
REF123 192.168.1.100 BC               # XLX module B links to REF123 module C
```

### How It Works

- XLX connects to the REF reflector as a DPlus client
- Connection uses the standard DPlus handshake: connect → echo → login → ack
- Voice traffic flows bidirectionally between the linked XLX module and the remote REF module
- **No AMBEd required for D-Star**: REF and D-Star both use AMBE+ codec, so voice data passes directly through XLXd
- DMR, YSF, NXDN, M17, and P25 users are transcoded via AMBEd as normal

### Module Mapping

Unlike YSF/NXDN/P25 peering which only supports one module, REF peering uses explicit module mapping:

| Config | Meaning |
|--------|---------|
| `EA` | XLX module E ↔ REF module A |
| `AA` | XLX module A ↔ REF module A |
| `BC` | XLX module B ↔ REF module C |

This allows you to link any XLX module to any REF module.

### Connection Sequence

```
XLX (client)                           REF (server)
    |                                       |
    |-------- Connect (0x05) -------------->|
    |                                       |
    |<------- Connect Echo -----------------|
    |                                       |
    |-------- Login (callsign) ------------>|
    |                                       |
    |<------- Login ACK (OKRW) -------------|
    |                                       |
    |<-------- Keepalive (0x03) ----------->|
    |           (every 5 seconds)           |
    |                                       |
    |<-------- Voice Frames --------------->|
    |                                       |
```

### Protocol Details

| Parameter | Value |
|-----------|-------|
| Port | 20001 (default) |
| Keepalive | Every 5 seconds |
| Timeout | 30 seconds |
| Voice Codec | AMBE+ (same as D-Star) |

### Notes

- REF peers appear alongside other peers in the dashboard
- The REF reflector must be accessible from your XLX server
- D-Star traffic flows directly without transcoding
- Other modes (DMR, YSF, M17, etc.) are transcoded via AMBEd
- XLX identifies itself using its configured callsign (with REF prefix)
- Connection state is logged: connecting → logging in → connected

---

## XRF/DExtra Reflector Peering

XLX can connect directly to XRF reflectors as a peer using the DExtra protocol, allowing bidirectional voice between XRF reflector users and XLX users (DMR, M17, YSF, NXDN, P25, etc.).

**Note:** XRF peering support is outbound only (XLX connects to XRF as a client). XRF reflectors cannot initiate connections to XLX via this mechanism.

### Configuration

Add XRF peers to your `xlxd.interlink` file:

```
# XRF/DExtra Reflector Peering
# Format: XRFnnn <hostname>[:port] <local><remote>
#
# - XRF callsign must be XRFnnn format (e.g., XRF001, XRF123)
# - Port defaults to 30001 if not specified
# - Module mapping: first letter = local XLX module, second = remote XRF module
# - Both module letters must be A-Z

XRF001 xrf001.example.com EA          # XLX module E links to XRF001 module A
XRF030 xrf030.dstargateway.org:30001 AA    # XLX module A links to XRF030 module A
XRF123 192.168.1.100 BC               # XLX module B links to XRF123 module C
```

### How It Works

- XLX connects to the XRF reflector as a DExtra client
- Connection uses the standard DExtra handshake with XRF revision 2 format
- Voice traffic flows bidirectionally between the linked XLX module and the remote XRF module
- **No AMBEd required for D-Star**: XRF and D-Star both use AMBE+ codec, so voice data passes directly through XLXd
- DMR, YSF, NXDN, M17, and P25 users are transcoded via AMBEd as normal

### Module Mapping

Like REF peering, XRF peering uses explicit module mapping:

| Config | Meaning |
|--------|---------|
| `EA` | XLX module E ↔ XRF module A |
| `AA` | XLX module A ↔ XRF module A |
| `BC` | XLX module B ↔ XRF module C |

This allows you to link any XLX module to any XRF module.

### Connection Sequence

```
XLX (client)                           XRF (server)
    |                                       |
    |-------- Connect (XRF rev 2) --------->|
    |                                       |
    |<------- Connect ACK ------------------|
    |                                       |
    |<-------- Keepalive ------------------>|
    |           (every 10 seconds)          |
    |                                       |
    |<-------- Voice Frames --------------->|
    |                                       |
```

### Protocol Details

| Parameter | Value |
|-----------|-------|
| Port | 30001 (default) |
| Keepalive | Every 10 seconds |
| Timeout | 60 seconds |
| Voice Codec | AMBE+ (same as D-Star) |

### Notes

- XRF peers appear alongside other peers in the dashboard
- The XRF reflector must be accessible from your XLX server
- D-Star traffic flows directly without transcoding
- Other modes (DMR, YSF, M17, etc.) are transcoded via AMBEd
- XLX identifies itself using its configured callsign (with XRF prefix)

---

## DCS Reflector Peering

XLX can connect directly to DCS reflectors as a peer using the DCS protocol, allowing bidirectional voice between DCS reflector users and XLX users (DMR, M17, YSF, NXDN, P25, etc.).

**Note:** DCS peering support is outbound only (XLX connects to DCS as a client). DCS reflectors cannot initiate connections to XLX via this mechanism.

### Configuration

Add DCS peers to your `xlxd.interlink` file:

```
# DCS Reflector Peering
# Format: DCSnnn <hostname>[:port] <local><remote>
#
# - DCS callsign must be DCSnnn format (e.g., DCS001, DCS123)
# - Port defaults to 30051 if not specified
# - Module mapping: first letter = local XLX module, second = remote DCS module
# - Both module letters must be A-Z

DCS001 dcs001.example.com EA          # XLX module E links to DCS001 module A
DCS030 dcs030.xreflector.net:30051 AA    # XLX module A links to DCS030 module A
DCS123 192.168.1.100 BC               # XLX module B links to DCS123 module C
```

### How It Works

- XLX connects to the DCS reflector as a DCS client
- Connection uses the standard DCS handshake with 519-byte connect packet
- Voice traffic flows bidirectionally between the linked XLX module and the remote DCS module
- **No AMBEd required for D-Star**: DCS and D-Star both use AMBE+ codec, so voice data passes directly through XLXd
- DMR, YSF, NXDN, M17, and P25 users are transcoded via AMBEd as normal

### Module Mapping

Like REF and XRF peering, DCS peering uses explicit module mapping:

| Config | Meaning |
|--------|---------|
| `EA` | XLX module E ↔ DCS module A |
| `AA` | XLX module A ↔ DCS module A |
| `BC` | XLX module B ↔ DCS module C |

This allows you to link any XLX module to any DCS module.

### Connection Sequence

```
XLX (client)                           DCS (server)
    |                                       |
    |-------- Connect (519 bytes) --------->|
    |                                       |
    |<------- Connect ACK (14 bytes) -------|
    |                                       |
    |<-------- Keepalive ------------------>|
    |           (every 5 seconds)           |
    |                                       |
    |<-------- Voice Frames --------------->|
    |                                       |
```

### Protocol Details

| Parameter | Value |
|-----------|-------|
| Port | 30051 (default) |
| Keepalive | Every 5 seconds |
| Timeout | 30 seconds |
| Connect Packet | 519 bytes |
| Voice Codec | AMBE+ (same as D-Star) |

### Notes

- DCS peers appear alongside other peers in the dashboard
- The DCS reflector must be accessible from your XLX server
- D-Star traffic flows directly without transcoding
- Other modes (DMR, YSF, M17, etc.) are transcoded via AMBEd
- XLX identifies itself using its configured callsign

---

## Troubleshooting

### M17 clients cannot connect
- Verify UDP port 17000 is open in firewall
- Check reflector logs for connection attempts
- Ensure client is using correct callsign format

### Audio quality issues on M17
- M17 uses software Codec2 transcoding (full fidelity)
- AMBE encode/decode uses DVSI hardware via AMBEd
- If audio is poor, check ambed connectivity and DVSI hardware

### Transcoding not working
- Verify AMBEd v1.4.0+ is running and connected
- Check console output for codec stream creation messages
- Ensure Codec2 library initialized correctly in ambed

### P25 peering issues
- Verify the P25 reflector hostname and port are correct in xlxd.interlink
- Check that outbound UDP to the P25 reflector port is not blocked
- AMBEd v1.4.0+ is required for IMBE software vocoder support
- Check ambed startup output for "Software vocoders : X IMBE (P25), X Codec2 (M17)"

### REF peering issues
- Verify the REF hostname and port are correct in xlxd.interlink (default port 20001)
- Check that outbound UDP to the REF port is not blocked by firewall
- Module string must be exactly 2 uppercase letters (e.g., "AA", "EA", "BC")
- Check logs for connection state: "DPlus connecting" → "connect echo" → "login ack - connected"
- If stuck at "connecting", the REF may be unreachable or port blocked
- If stuck at "logging in", the REF may be rejecting the login (check REF's whitelist)
- If "login nack" received, your callsign may not be authorized on that REF

### XRF peering issues
- Verify the XRF hostname and port are correct in xlxd.interlink (default port 30001)
- Check that outbound UDP to the XRF port is not blocked by firewall
- Module string must be exactly 2 uppercase letters (e.g., "AA", "EA", "BC")
- Check logs for "DExtra connecting" → "connect ack - connected"
- If stuck at "connecting", the XRF may be unreachable or port blocked
- If connect timeout occurs, the XRF may not be accepting connections

### DCS peering issues
- Verify the DCS hostname and port are correct in xlxd.interlink (default port 30051)
- Check that outbound UDP to the DCS port is not blocked by firewall
- Module string must be exactly 2 uppercase letters (e.g., "AA", "EA", "BC")
- Check logs for "DCS peer connecting" → "DCS peer connected"
- If stuck at "connecting", the DCS reflector may be unreachable or port blocked
- If connect timeout occurs, the DCS may not be accepting connections
- DCS uses 519-byte connect packets - ensure no packet fragmentation issues

---

## License

GPL v3 - See LICENSE file for details.

---

## Credits

### XLX Reflector
- **Jean-Luc Deltombe (LX3JL)** - XLX original author
- **Luc Engelmann (LX1IQ)** - XLX contributor

### M17 Protocol Implementation
- **Andy Taylor (MW0MWZ)** - M17 native support implementation for XLX
  - Native M17 reflector connectivity
  - Codec2 software transcoding integrated with AMBEd
- **M17 Project** - M17 protocol specification (https://m17project.org)

### YSF Reflector Peering
- **Andy Taylor (MW0MWZ)** - YSF reflector peering implementation
  - Native YSF protocol peering support
  - Bidirectional voice bridging between YSF reflectors and XLX
- **Jonathan Naylor (G4KLX)** - MMDVMHost (https://github.com/g4klx/MMDVMHost)
  - YSF protocol reference implementation

### NXDN Reflector Peering
- **Andy Taylor (MW0MWZ)** - NXDN reflector peering implementation
  - Native NXDN protocol peering support
  - Bidirectional voice bridging between NXDN reflectors and XLX
- **Jonathan Naylor (G4KLX)** - MMDVMHost (https://github.com/g4klx/MMDVMHost)
  - NXDN protocol reference implementation
- **Andy Uribe (CA6JAU)** - NXDN cross-mode gateway implementations
  - NXDN2DMR (https://github.com/juribeparada/NXDN2DMR)
  - DMR2NXDN (https://github.com/juribeparada/DMR2NXDN)
- **Doug McLain (AD8DP)** - Current maintainer of NXDN cross-mode gateways
  - Protocol handling and cross-mode patterns

### P25 Reflector Peering
- **Andy Taylor (MW0MWZ)** - P25 reflector peering implementation
  - Native P25 protocol peering support
  - Bidirectional voice bridging between P25 reflectors and XLX
- **Jonathan Naylor (G4KLX)** - MMDVMHost (https://github.com/g4klx/MMDVMHost)
  - P25 protocol reference implementation
- **Doug McLain (AD8DP)** - P25 cross-mode gateway implementations
  - DMR2P25 and P252DMR (https://github.com/nostar)
  - Cross-mode patterns and protocol handling inspiration

### REF/DPlus Reflector Peering
- **Andy Taylor (MW0MWZ)** - REF/DPlus reflector peering implementation
  - Native DPlus protocol peering support (XLX as client to REF)
  - Bidirectional voice bridging between REF reflectors and XLX
  - Module mapping support (any XLX module to any REF module)
- **Jonathan Naylor (G4KLX)** - ircDDBGateway (https://github.com/g4klx/ircDDBGateway)
  - DPlus protocol reference implementation

### XRF/DExtra Reflector Peering
- **Andy Taylor (MW0MWZ)** - XRF/DExtra reflector peering implementation
  - Native DExtra protocol peering support (XLX as client to XRF)
  - Bidirectional voice bridging between XRF reflectors and XLX
  - Module mapping support (any XLX module to any XRF module)
- **Jonathan Naylor (G4KLX)** - ircDDBGateway (https://github.com/g4klx/ircDDBGateway)
  - DExtra protocol reference implementation

### DCS Reflector Peering
- **Andy Taylor (MW0MWZ)** - DCS reflector peering implementation
  - Native DCS protocol peering support (XLX as client to DCS)
  - Bidirectional voice bridging between DCS reflectors and XLX
  - Module mapping support (any XLX module to any DCS module)
- **Jonathan Naylor (G4KLX)** - ircDDBGateway (https://github.com/g4klx/ircDDBGateway)
  - DCS protocol reference implementation

### Voice Codecs

#### Codec2 (M17 Voice Codec)
- **David Rowe (VK5DGR)** - Codec2 library author (https://github.com/drowe67/codec2)
- Open source voice codec used by M17 protocol
- 3200bps mode provides 8 bytes per 20ms voice frame

#### IMBE (P25 Voice Codec)
- **OP25 Project by Osmocom** - Software IMBE vocoder (https://github.com/osmocom/op25)
- 88-bit frames (11 bytes) per 20ms voice frame
- Used for P25 reflector peering support

### Reference Projects

| Project | Author | Used For |
|---------|--------|----------|
| [Codec2](https://github.com/drowe67/codec2) | VK5DGR | M17 voice codec |
| [OP25](https://github.com/osmocom/op25) | Osmocom | P25 IMBE voice codec |

---

(c) 2016 Jean-Luc Deltombe (LX3JL) and Luc Engelmann (LX1IQ)

