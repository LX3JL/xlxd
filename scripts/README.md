# Scripts

Service files and helper scripts for running XLXd and AMBEd.

| File | What it is |
|------|------------|
| `xlxd.service` | systemd unit for XLXd |
| `ambed.service` | systemd unit for AMBEd |
| `xlxd` | SysV init script for XLXd, for systems without systemd |
| `ambed` | SysV init script for AMBEd, for systems without systemd |
| `tune-xlxd.sh` | Network and process priority tuning for low-latency voice |

Both daemons expect the standard install locations:

- XLXd: `/xlxd/xlxd`, settings in `/xlxd/xlxd.ini`
- AMBEd: `/ambed/ambed`, settings in `/ambed/ambed.ini`

`make install` puts them there, and copies a sample `.ini` if there isn't one already.

## XLXd with systemd

Set at least `CALLSIGN` and `LISTEN_IP` in `/xlxd/xlxd.ini`, then:

```bash
cp xlxd.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now xlxd
```

XLXd writes its own process ID to `/var/log/xlxd.pid`, which the dashboard
reads. If you change `PID_PATH` in `xlxd.ini`, change `PIDFile=` in the unit to
match.

XLXd logs to syslog. To follow it:

```bash
journalctl -u xlxd -f
```

The unit restarts XLXd if it crashes, but not if it exits with an error, for
example a missing or broken `xlxd.ini`. Check the log if it won't start.

## AMBEd with systemd

```bash
cp ambed.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now ambed
```

The unit unloads the kernel's FTDI driver, the `ftdi_sio` module, before
starting, so the FTDI D2XX driver can claim the AMBE dongles. See `ambed/README.md` for
the driver setup and `ambed.ini`.

## Managing the services

```bash
systemctl status xlxd       # is it running
systemctl restart xlxd      # restart it
systemctl stop xlxd         # stop it
systemctl disable xlxd      # don't start it at boot
```

The same commands work for `ambed`.

Most settings in `xlxd.ini` and `ambed.ini` are picked up within 3 minutes
without a restart. Each `.ini` says which settings need one.

## XLXd without systemd

On systems that still use SysV init:

```bash
cp xlxd /etc/init.d/xlxd
chmod +x /etc/init.d/xlxd
update-rc.d xlxd defaults
service xlxd start
```

It runs `/xlxd/xlxd /xlxd/xlxd.ini`. Edit `DAEMON` and `ARGUMENTS` at the top of
the script if yours are elsewhere.

## AMBEd without systemd

```bash
cp ambed /etc/init.d/ambed
chmod +x /etc/init.d/ambed
update-rc.d ambed defaults
service ambed start
```

It runs `/ambed/ambed /ambed/ambed.ini`. Edit `DAEMON` and `ARGUMENTS` at the
top of the script if yours are elsewhere.

AMBEd runs in the foreground and writes its log to its output, so the script
runs it in the background and sends the log to syslog, tagged `ambed`. Like
the systemd unit, it unloads the `ftdi_sio` module before starting, so the
FTDI D2XX driver can claim the AMBE dongles. `stop` waits up to 10 seconds
for AMBEd to exit, then kills it, so a restart doesn't find the port or the
dongles still in use.

## Unresponsive AMBE dongles

AMBEd recovers a USB AMBE dongle that stops responding by itself. It logs
`Device <name>:<serial> is unresponsive, trying to re-open it...`, then closes,
re-opens and resets the dongle without restarting. If AMBEd itself crashes,
the systemd unit restarts it. No cron job is needed.

## System tuning

`tune-xlxd.sh` tunes the network stack and process priority for UDP voice
traffic: process priority, sysctl network buffers, NIC interrupt coalescing,
queue discipline and socket buffers.

```bash
./tune-xlxd.sh                    # show what it would change, changes nothing
sudo ./tune-xlxd.sh --apply       # apply the changes until the next reboot
sudo ./tune-xlxd.sh --apply --persist   # also keep the sysctl changes across reboots
```

It finds the network interface by itself. Use `--interface eth1` to pick
another one. `--persist` writes `/etc/sysctl.d/99-xlxd.conf`.
