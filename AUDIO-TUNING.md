# Audio Tuning Guide

How to set the gains in AMBEd and XLXd so that every mode sounds equally loud,
whichever mode a station transmits on and whichever mode it is heard on.

## How it fits together

Audio passes through two sets of gains, once on the way in and once on the
way out.

**Incoming**, from a radio or peer to the AGC:

```
  FROM THE RADIO     XLXd (xlxd.ini)                  AMBEd (ambed.ini)

  YSF     ──────▶  YSF_TO_DMR  ──┐
  NXDN    ──────▶  NXDN_TO_DMR ──┤
  DMR     ───────────────────────┴──▶  DMR AMBE ──▶  DMR_TO_PCM   ──┐
  D-Star  ────────────────────────────────────────▶  DSTAR_TO_PCM ──┤
  M17     ────────────────────────────────────────▶  M17_TO_PCM   ──┼──▶ filter ──▶ AGC
  P25     ────────────────────────────────────────▶  P25_TO_PCM   ──┘
```

**Outgoing**, from the AGC to the listening radios:

```
          AMBEd (ambed.ini)                 XLXd (xlxd.ini)          TO THE RADIO

  AGC ──┬──▶  PCM_TO_DMR   ──▶  DMR AMBE  ──┬─────────────────────────▶  DMR
        │                                   ├──▶  DMR_TO_YSF  ────────▶  YSF
        │                                   └──▶  DMR_TO_NXDN ────────▶  NXDN
        ├──▶  PCM_TO_DSTAR ───────────────────────────────────────────▶  D-Star
        ├──▶  PCM_TO_M17   ───────────────────────────────────────────▶  M17
        └──▶  PCM_TO_P25   ───────────────────────────────────────────▶  P25
```

DMR, YSF and NXDN share a codec, so audio between those three modes doesn't go
through AMBEd at all. It goes straight from `DMR AMBE` on the incoming side to
`DMR AMBE` on the outgoing side, and only the XLXd gains apply.

- **YSF and NXDN** share DMR's codec. XLXd converts them to DMR AMBE as they
  arrive, and `YSF_TO_DMR` and `NXDN_TO_DMR` in `xlxd.ini` set their level on
  the way in. AMBEd only ever sees them as DMR. On the way out, `DMR_TO_YSF`
  and `DMR_TO_NXDN` set the level YSF and NXDN listeners hear.
- **D-Star, DMR, M17 and P25** are decoded by AMBEd. `<MODE>_TO_PCM` in
  `ambed.ini` sets each mode's level going into the AGC, and `PCM_TO_<MODE>`
  sets the level it is encoded at on the way out.
- **The AGC** in AMBEd evens out the differences between stations. If the
  gains are right, a station with a well set-up radio needs no correction from
  the AGC at all. That is what this guide tunes for: **an AGC gain average of
  0 dB** for a typical station on every mode.

So the work splits in two:

| Mode | Incoming level set by | Outgoing level set by |
|------|-----------------------|-----------------------|
| D-Star | `DSTAR_TO_PCM` in `ambed.ini` | `PCM_TO_DSTAR` in `ambed.ini` |
| DMR | `DMR_TO_PCM` in `ambed.ini` | `PCM_TO_DMR` in `ambed.ini` |
| M17 | `M17_TO_PCM` in `ambed.ini` | `PCM_TO_M17` in `ambed.ini` |
| P25 | `P25_TO_PCM` in `ambed.ini` | `PCM_TO_P25` in `ambed.ini` |
| YSF | `YSF_TO_DMR` in `xlxd.ini` | `DMR_TO_YSF` in `xlxd.ini` |
| NXDN | `NXDN_TO_DMR` in `xlxd.ini` | `DMR_TO_NXDN` in `xlxd.ini` |

`DMR_TO_PCM` and `PCM_TO_DMR` also apply to YSF and NXDN audio going through
AMBEd, so set DMR first, then bring YSF and NXDN into line with it in XLXd.

## Before you start

### Turn on the tuning log

In `ambed.ini`:

```ini
[Logging]
TUNING_LOG=1

[AGC]
USE_AGC=1
```

Leave the other AGC settings at their defaults while tuning.

AMBEd writes one report every time a transmission ends. To watch them:

```bash
journalctl -u ambed -f | grep Tuning:
```

### Set up the test radios

The results are only as good as the radios you test with. Set the mic gain to
a typical setting, so the result suits the average station:

| Radio's mic gain control | Set it to |
|--------------------------|-----------|
| 5 steps | 4 |
| 3 steps | the middle step |
| A knob or a continuous setting | just above the middle |

NXDN radios are the exception, and the two main makes differ:

| NXDN radio | Set it up like this |
|------------|---------------------|
| Kenwood NX / NEXEDGE | Very hot audio. Turn off the radio's transmit AGC and set the mic gain to -2 dB or -4 dB |
| Icom | Mic gain at maximum |

This gets them closer, but Kenwood and Icom NXDN radios still don't sound
quite the same level, so tune `NXDN_TO_DMR` with the make most of your NXDN
users have.

Talk at your normal distance from the microphone, in your normal voice.

**Transmit for at least 10 seconds every time.** The AGC figures in the report
are averages over the whole transmission, and short overs skew them.

### Test properly

- **Talk for at least 10 seconds of continuous speech** in each test. The AGC
  starts every transmission at 0 dB and needs time to settle, so short overs
  give misleading numbers. 10 seconds is 500 voice frames, so check the
  `voice frames` count in the first line of the report is at least 500.
- **Use a real radio**, not recorded audio.
- **Test one mode at a time**, so you know which report is which. The
  callsign in the report is always the reflector's, because AMBEd isn't told
  who is talking.
- **Do several overs** on each mode and go by the typical result, not one
  over.

## Reading the report

> **The only number you tune by is the `gain average` on the `AGC` line.**
> It is marked `^^^^ <-- THIS ONE` under each example below. Aim for 0 dB.

A report from a well-tuned DMR transmission:

```
Tuning: stream 4127 XLX999   DMR input, 612 voice frames of 705
Tuning:   decoded               level  -31.4 dBFS  peak   -9.8 dBFS  clipped 0.00%
Tuning:   DMR_TO_PCM -1.50      level  -32.9 dBFS  peak  -11.3 dBFS  clipped 0.00%
Tuning:   AGC                   target -41.0 dBFS  speech peaks -41.2 dBFS  gain average +0.2 dB, -2.1 to +1.8 dB  at limit 0.0% high 0.0% low  limiter 0.00%
                                                                            ^^^^^^^^^^^^^^^^^^^^  <-- THIS ONE
Tuning:   after AGC             level  -32.7 dBFS  peak  -11.6 dBFS  clipped 0.00%
Tuning:   PCM_TO_DSTAR +11.50   level  -21.2 dBFS  peak   -1.6 dBFS  clipped 0.00%
Tuning:   PCM_TO_M17 +5.25      level  -27.4 dBFS  peak   -6.4 dBFS  clipped 0.00%
Tuning:   PCM_TO_P25 +0.75      level  -31.9 dBFS  peak  -10.9 dBFS  clipped 0.00%
```

What to look for:

| Where | What it tells you | What you want |
|-------|-------------------|---------------|
| First line: `DMR input` | The mode AMBEd decoded. YSF and NXDN show as `DMR` | The mode you're testing |
| First line: `612 voice frames` | How much speech was measured | 500 or more |
| AGC line: **`gain average +0.2 dB`** | **How much the AGC had to correct this station, on average** | **Close to 0 dB, within about ±1 dB** |
| AGC line: `-2.1 to +1.8 dB` | The range the AGC moved through | A few dB either side of 0 |
| AGC line: `at limit 0.0% high 0.0% low` | How long the AGC was at its limit | 0%, or close to it |
| `clipped` on every line | Audio hitting full scale | 0.00% |

The **gain average** is the number you tune by:

- **Positive** (for example `+4.6 dB`): the AGC is boosting, so this mode
  arrives too quiet. Raise its incoming gain.
- **Negative** (for example `-3.0 dB`): the AGC is cutting, so this mode
  arrives too loud. Lower its incoming gain.

## Step 1: D-Star, DMR, M17 and P25 incoming gains (AMBEd)

For each of these modes, do your test overs and read the gain average. Then
change that mode's `<MODE>_TO_PCM` in `ambed.ini` by the same amount:

| Typical gain average | Change |
|----------------------|--------|
| `+4.6 dB` | raise `<MODE>_TO_PCM` by 4.6 |
| `-3.0 dB` | lower `<MODE>_TO_PCM` by 3.0 |

For example, with `DMR_TO_PCM=-1.5` and a typical gain average of `+2.0 dB`,
set `DMR_TO_PCM=0.5`.

AMBEd checks `ambed.ini` every 3 minutes. Wait for this line in the log before
testing again:

```
Config: reloading /ambed/ambed.ini, changes apply from the next stream
```

Repeat until every mode's typical gain average is within about ±1 dB of 0.
**Do DMR first**, because YSF and NXDN are matched to it in the next step.

## Step 2: YSF and NXDN incoming gains (XLXd)

YSF and NXDN reach AMBEd as DMR, so their reports say `DMR input`. Once DMR
sits at 0 dB, a YSF or NXDN station should too. If it doesn't, the difference
is down to XLXd's `YSF_TO_DMR` or `NXDN_TO_DMR`, not AMBEd.

A report from a YSF radio arriving too quiet:

```
Tuning: stream 4131 XLX999   DMR input, 688 voice frames of 760
Tuning:   decoded               level  -36.0 dBFS  peak  -14.1 dBFS  clipped 0.00%
Tuning:   DMR_TO_PCM -1.50      level  -37.5 dBFS  peak  -15.6 dBFS  clipped 0.00%
Tuning:   AGC                   target -41.0 dBFS  speech peaks -45.6 dBFS  gain average +4.6 dB, +2.0 to +7.9 dB  at limit 0.0% high 0.0% low  limiter 0.00%
                                                                            ^^^^^^^^^^^^^^^^^^^^  <-- THIS ONE
Tuning:   after AGC             level  -33.0 dBFS  peak  -11.9 dBFS  clipped 0.00%
```

The AGC is boosting by 4.6 dB on average, so YSF needs to come up. Raise
`YSF_TO_DMR` in `xlxd.ini`, and lower it if the gain average is negative.

**Change these in small steps** and test again after each one. Small changes
here make a big difference, and the AGC reading doesn't change by the same
number of dB as the setting, so don't copy the gain average across the way you
do for AMBEd. Repeat until YSF and NXDN show the same gain average as DMR,
which is close to 0 dB.

XLXd also checks `xlxd.ini` every 3 minutes, and logs
`Config: reloading /xlxd/xlxd.ini` when it picks up a change.

NXDN is peer-only in XLXd, so to test it you need an NXDN radio on an NXDN
reflector peered with one of your modules.

## Step 3: Outgoing gains

Once every mode arrives at the same level, set how loud each mode is on air.
The log can't tell you how a radio sounds, so this step is done by ear.

Audio between stations on the same mode isn't changed by AMBEd. A D-Star
station heard on a D-Star radio is untouched, so it makes a good reference for
that radio.

### D-Star, DMR, M17 and P25 (AMBEd)

For each of these modes, listen on a radio on that mode:

1. Listen to a station on the same mode as the radio. That's the reference.
2. Listen to stations on the other modes, at the same volume setting.
3. Adjust that mode's outgoing gain until the other modes sound as loud as the
   reference:

| Listening radio | Setting in `ambed.ini` |
|-----------------|------------------------|
| D-Star | `PCM_TO_DSTAR` |
| DMR | `PCM_TO_DMR` |
| M17 | `PCM_TO_M17` |
| P25 | `PCM_TO_P25` |

After changing a `PCM_TO_<MODE>`, check its line in the next report shows
`clipped 0.00%`. If it doesn't, back the gain off.

YSF and NXDN stations heard on a DMR radio don't go through AMBEd. Their level
there was set in step 2.

### YSF and NXDN (XLXd)

`DMR_TO_YSF` applies to everything a YSF radio hears from the reflector,
including other YSF stations, so it can't balance one mode against another on
a YSF radio. Step 2 has already done that. It sets how loud the reflector is
overall on a YSF radio.

Set it so the reflector sounds as loud on a YSF radio as YSF normally does,
for example as loud as a YSF station heard directly or on another YSF
reflector. `DMR_TO_NXDN` works the same way for NXDN peers.

Change both in small steps, as in step 2.

## When a result doesn't fit

A report from a D-Star radio with its mic gain far too high:

```
Tuning: stream 4140 XLX999   DSTAR input, 540 voice frames of 612
Tuning:   decoded               level  -14.9 dBFS  peak   -0.4 dBFS  clipped 0.31%
Tuning:   DSTAR_TO_PCM -10.00   level  -24.9 dBFS  peak  -10.4 dBFS  clipped 0.00%
Tuning:   AGC                   target -41.0 dBFS  speech peaks -32.3 dBFS  gain average -8.8 dB, -9.0 to -5.2 dB  at limit 0.0% high 71.3% low  limiter 0.00%
                                                                            ^^^^^^^^^^^^^^^^^^^^  <-- THIS ONE
Tuning:   after AGC             level  -33.6 dBFS  peak  -12.3 dBFS  clipped 0.00%
```

- `at limit ... 71.3% low`: the AGC spent most of the over at its lowest
  setting and still couldn't bring the station down far enough.
- `decoded ... clipped 0.31%`: the audio was already clipping as it came
  out of the radio.

This is a radio problem, not a reflector one. Don't tune the reflector to suit
it: turn the radio's mic gain down and test again. The same goes for a station
stuck at the `high` limit, whose mic gain is too low.

A report that is too short to use:

```
Tuning: stream 4152 XLX999   M17 input, 176 voice frames of 240
```

176 voice frames is about 3.5 seconds of speech. Ignore it and talk for longer.

If every mode ends up a similar amount above or below 0 dB, even with
well-set-up radios, the AGC target may suit your stations better a little
higher or lower. See `AGC_TARGET` in `ambed/README.md`.

## Quick checklist

1. `TUNING_LOG=1` and `USE_AGC=1` in `ambed.ini`, AGC settings at their
   defaults.
2. Test radios: mic gain at 4 of 5, the middle of 3, or just above the middle
   of a knob. NXDN: Kenwood with transmit AGC off and mic gain at -2 or
   -4 dB, Icom with mic gain at maximum.
3. At least 10 seconds of speech per over (500+ voice frames), one mode at a
   time, several overs.
4. Tune `DMR_TO_PCM` until DMR's gain average is about 0 dB.
5. Tune `DSTAR_TO_PCM`, `M17_TO_PCM` and `P25_TO_PCM` the same way.
6. Tune `YSF_TO_DMR` and `NXDN_TO_DMR` in `xlxd.ini`, in small steps, until
   YSF and NXDN match DMR.
7. Set `PCM_TO_<MODE>` by ear against same-mode audio, and check for
   `clipped 0.00%`.
8. Set `DMR_TO_YSF` and `DMR_TO_NXDN` by ear for the overall level on YSF and
   NXDN radios.
