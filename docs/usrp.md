# USRP endpoints

USRP connects an analog front end, such as AllStar `app_rpt` (`chan_usrp`),
directly to an XLX module. Audio is raw PCM with PTT signaling over UDP; the
transcoder converts it to and from the digital codecs of the other clients.

## Requirements

- A transcoder (ambed) that answers the `AMBEDCAPS` request with PCM
  (codec 5) in its codec mask. xlxd probes it once per transcoder
  connection. Without it, PCM coming from USRP is not transcoded (logged
  once) and USRP endpoints receive nothing from digital clients; all other
  traffic keeps the legacy transcoder protocol.
- When the transcoder offers fewer outputs than a stream needs, PCM is the
  output dropped first (never happens with a 4-output transcoder).
- With no `/xlxd/xlxd.usrp` file (or an empty one) nothing changes: no USRP
  socket is opened and no `AMBEDCAPS` request is sent.

## Configuration

Copy the rows you need from [config/xlxd.usrp.example](../config/xlxd.usrp.example)
to `/xlxd/xlxd.usrp`. One row per module:

```text
# module bind_ip bind_port peer_ip peer_port source_callsign
A 127.0.0.1 34001 127.0.0.1 32001 N0CALL
```

- `bind_ip:bind_port` is the xlxd side, `peer_ip:peer_port` the app_rpt side.
- `source_callsign` is shown as the talker for audio coming from the front end.
- Invalid rows are logged and ignored: bad module/address/port/callsign,
  duplicate module, duplicate peer, bind clash (including `0.0.0.0` on the
  same port) or bind equal to peer.
- The file is read at startup only.

The matching `app_rpt` node (here private node 1999 for module A):

```ini
[1999]
rxchannel = USRP/127.0.0.1:34001:32001   ; USRP/<xlxd_ip>:<bind_port>:<peer_port>
duplex = 0
hangtime = 0
telemdefault = 0
```

AllStar callers connect to node 1999; xlxd only sees the one USRP peer.

## Behavior

- Frames: 32-byte header (`USRP`, then big-endian seq, memory, keyup,
  talkgroup, type, mpxid, reserved) followed for voice by 320 bytes of
  S16LE 8 kHz mono PCM (20 ms). Only voice is accepted: 352-byte keyed
  frames and 32-byte unkey frames, all other header fields zero. Other
  frames (DTMF, text, TLV) are dropped; the first one per endpoint is logged.
- Only packets from the exact configured peer address and port are accepted.
- Receive: the first keyed frame opens a stream on the module (the module's
  first talker keeps it, as for every other protocol). Unkey, or 100 ms
  without voice, ends the stream.
- Transmit: frames of the module that carry PCM are sent as keyed voice; the
  end of the stream, or 100 ms without PCM, sends an unkey. A stream is never
  sent back to the USRP peer it came from.
- The USRP endpoint is a permanent client (protocol `USRP`) of its module and
  shows on the dashboard as a permanent node with the callsign of its row.

## Security

USRP has no login and no authentication. Bind to loopback or a private
interface and run app_rpt on the same host or a trusted network; anything
that can spoof the peer address can talk on the module.
