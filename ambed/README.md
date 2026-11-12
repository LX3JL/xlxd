# AMBEd

AMBEd is the transcoder daemon for XLXd. It converts voice between the digital
modes on a reflector, using DVSI hardware for AMBE and software vocoders for
Codec2 and IMBE. It also evens out levels, so stations sound alike whichever
mode they use.

Version 1.5.0

## Codecs

| Codec | Modes | Vocoder |
|---|---|---|
| AMBE+ | D-Star | DVSI hardware |
| AMBE2+ | DMR, YSF, NXDN | DVSI hardware |
| Codec2 3200 bps | M17 | Software |
| IMBE | P25 | Software |

DMR, YSF and NXDN share a codec, so voice between them isn't transcoded and
AMBEd sees all three as DMR.

## Transcoding paths

| From | To | How |
|---|---|---|
| D-Star | DMR/YSF/NXDN | Hardware decode, hardware encode |
| DMR/YSF/NXDN | D-Star | Hardware decode, hardware encode |
| D-Star, DMR/YSF/NXDN | M17, P25 | Hardware decode, software encode |
| M17 | D-Star, DMR/YSF/NXDN | Software decode, hardware encode |
| M17 | P25 | Software decode, software encode |
| P25 | D-Star, DMR/YSF/NXDN | Software decode, hardware encode |
| P25 | M17 | Software decode, software encode |

An M17 or P25 input uses 2 hardware channels, one for each AMBE encoder.

## Hardware

- DF2ET's [AMBE3003USB](https://github.com/phl0/AMBE3003USB)
- LX3JL's [USB-3006](https://github.com/lx3jl/usb-3006)
- DVSI USB-3000, USB-3003 and USB-3012
- NWDR ThumbDV and ThumbDV-3
- DVMEGA AMBE3000 and AMBE3003

| Device | Hardware channels | Transcoding channels | Software vocoders (IMBE and Codec2) |
|---|---|---|---|
| 3000 (pair) | 2 | 2 | 1 each |
| 3003 (single) | 3 | 2 | 1 each |
| 3003 (pair) | 6 | 6 | 3 each |
| 3003 + 3000 (pair) | 4 | 4 | 2 each |
| 3006 | 6 | 6 | 3 each |
| 3012 | 12 | 12 | 6 each |

- USB-3000 devices must be used in pairs, or paired with a USB-3003.
- Several devices can be used at once.
- Avoid USB hubs, they can be unreliable.
- The maximum number of concurrent streams is worked out from the hardware.

## Installation

1. Install the FTDI D2XX drivers from the FTDI website and follow their
   instructions. Tested on 1.4.6.

2. Install the build tools:

   ```bash
   apt-get install build-essential
   ```

3. Build and install:

   ```bash
   git clone https://github.com/LX3JL/xlxd.git
   cd xlxd/ambed
   make clean
   make
   make install
   ```

   This installs to `/ambed`. `ambed.ini` is only copied if there isn't one
   already, so your tuning is kept.

Always run `make clean` first after updating. The makefile doesn't track
header changes, so a plain `make` can link old and new code together.

## Running

```
ambed [ambed.ini]
```

Everything is set in `ambed.ini`. Set `LISTEN_IP` first: `127.0.0.1` if xlxd is on
the same machine, otherwise the machine's own address. Without the file AMBEd
uses the defaults in `main.h` and listens on `127.0.0.1`.

AMBEd must run as root because of the FTDI driver, and the `ftdi_sio` and
`usbserial` kernel modules must be unloaded. Plug in at least one device first.

To run it as a service, copy `../scripts/ambed.service` to
`/etc/systemd/system/`:

```ini
[Unit]
Description=AMBE Transcoder Daemon
After=network.target

[Service]
Type=simple
User=root
Group=root
ExecStartPre=-/sbin/rmmod ftdi_sio
ExecStartPre=-/sbin/rmmod usb_serial
ExecStart=/ambed/ambed /ambed/ambed.ini
Restart=on-abnormal

[Install]
WantedBy=multi-user.target
```

Then enable and start it:

```bash
systemctl daemon-reload
systemctl enable --now ambed
```

Or start it by hand with `/ambed/run`.

At startup AMBEd finds and configures every device, then reports:

```
Starting AMBEd 1.5.0
Codec interfaces initialized successfully : 6 channels available
Software vocoders : 3 IMBE (P25), 3 Codec2 (M17)
Max concurrent streams: 3 (from 6 vocoder channels)
AMBEd started and listening on 127.0.0.1:10100
Voice packets marked as DSCP 46
```

## Audio processing

Every path runs through the same chain:

```
decode -> <MODE>_TO_PCM -> bandpass filter -> AGC -> PCM_TO_<MODE> -> encode
```

- **`<MODE>_TO_PCM`** corrects what each mode's decoder does to the level, so
  every mode reaches the AGC at about the same level.
- **Bandpass filter** keeps 300-3400 Hz.
- **AGC** corrects what each station does to the level: mic gain, radio and
  voice.
- **`PCM_TO_<MODE>`** sets the level for each encoder.

### AGC

The AGC works like the one on an HF receiver: fast down, hold through gaps, then
up, faster the further a station is below the target.

- It follows the speech peaks, the loud parts of words, so it levels the speaker
  and leaves the shape of the speech alone for the vocoders.
- Pauses and noise below `AGC_GATE` leave the gain where it is.
- Each stream starts at 0 dB, so very short transmissions aren't fully levelled.
- A peak limiter leaves room for the highest `PCM_TO_<MODE>`, so a boost can't
  clip an encoder.

## Tuning

`ambed.ini` overrides the defaults in `main.h` without a rebuild. AMBEd checks it
every 3 minutes. Most changes apply from the next stream, so a transmission
already running keeps its levels, and DSCP changes apply at once.
`LISTEN_IP` and `LISTEN_PORT` need a restart. A setting left out uses its default.
A bad value is logged and ignored, and the value in use stays.

| Setting | Default | Meaning |
|---|---|---|
| `LISTEN_IP` | 127.0.0.1 | IP to bind to |
| `LISTEN_PORT` | 10100 | UDP port, xlxd's `TRANSCODER_PORT` must match |
| `DSCP_MARKING_ENABLE` | 1 | Mark voice packets for QoS |
| `DSCP_VALUE` | 46 | DSCP value, 0-63, 46 is EF |
| `STREAM_ACTIVITY_TIMEOUT` | 3 s | Close a stream after this long with no packets |
| `TUNING_LOG` | 1 | Log levels when a stream closes |
| `DSTAR_TO_PCM` | -10 dB | D-Star decode gain |
| `DMR_TO_PCM` | -1.5 dB | DMR, YSF and NXDN decode gain |
| `M17_TO_PCM` | -10.75 dB | M17 decode gain |
| `P25_TO_PCM` | -3.75 dB | P25 decode gain |
| `PCM_TO_DSTAR` | +11.5 dB | D-Star encode gain |
| `PCM_TO_DMR` | 0 dB | DMR, YSF and NXDN encode gain |
| `PCM_TO_M17` | +5.25 dB | M17 encode gain |
| `PCM_TO_P25` | +0.75 dB | P25 encode gain |
| `USE_AGC` | 1 | AGC on or off |
| `AGC_TARGET` | -41 dBFS | Speech peak level to aim for |
| `AGC_CLAMPING` | 9 dB | Most the gain can move either way, 0 leaves the audio alone |
| `AGC_ATTACK` | 150 ms | How fast the gain comes down |
| `AGC_HANG` | 1000 ms | How long the gain holds before it can rise |
| `AGC_RECOVERY` | 2 | How fast the gain rises, dB/s for each dB below the target |
| `AGC_RECOVERY_MIN` | 3 dB/s | Slowest rise |
| `AGC_RECOVERY_MAX` | 20 dB/s | Fastest rise |
| `AGC_GATE` | -60 dBFS | Frames quieter than this don't move the gain |
| `USE_BANDPASSFILTER` | 1 | Bandpass filter on or off |

### Tuning log

When a stream closes, AMBEd logs the voice level at each stage. Pauses aren't
counted, and a level is the mean absolute sample value. Set `TUNING_LOG=0` to
turn this off.

```
Tuning: stream 1 XLX672   M17 input, 440 voice frames of 540
Tuning:   decoded               level  -30.5 dBFS  peak   -8.9 dBFS  clipped 0.00%
Tuning:   M17_TO_PCM -10.75     level  -45.6 dBFS  peak  -19.9 dBFS  clipped 0.00%
Tuning:   AGC                   target -41.0 dBFS  speech peaks -43.6 dBFS  gain average -3.7 dB, -4.5 to +0.4 dB  at limit 0.0% high 0.0% low  limiter 0.00%
Tuning:   after AGC             level  -49.3 dBFS  peak  -23.7 dBFS  clipped 0.00%
Tuning:   PCM_TO_DSTAR +11.50   level  -37.8 dBFS  peak  -12.2 dBFS  clipped 0.00%
Tuning:   PCM_TO_DMR +0.00      level  -49.3 dBFS  peak  -23.7 dBFS  clipped 0.00%
Tuning:   PCM_TO_P25 +0.75      level  -48.5 dBFS  peak  -22.9 dBFS  clipped 0.00%
```

The callsign shown is the reflector's, as AMBEd isn't told who is talking.

### How to tune

1. Look at several stations on each mode, not just your own radio.
2. Set each `<MODE>_TO_PCM` so typical stations on that mode average near 0 dB
   of AGC gain.
3. If most modes sit high or low, move `AGC_TARGET`.
4. A station at one limit for most of a stream is too far off for the AGC to
   correct.
5. Use `PCM_TO_<MODE>` to match how each mode sounds on air, and back one off if
   it shows clipping.

To see levels without changing the audio, set `AGC_CLAMPING=0`. Speech peaks are
still logged.

## Build-time options

These are in `main.h`:

| Setting | Default | Meaning |
|---|---|---|
| `CONFIG_RELOAD_PERIOD` | 180 s | How often `ambed.ini` is checked |

## License

Created by Jean-Luc Deltombe (LX3JL). AMBEd is part of xlxd, free software under
the GNU General Public License v3 or later.
