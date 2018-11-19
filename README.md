# Copyright

© 2016 Luc Engelmann LX1IQ
The XLX Multiprotocol Gateway Reflector Server is part of the software system
for the D-Star Network.
The sources are published under GPL Licenses.
 
# Usage
 
The packages which are described in this document are designed to install server
software which is used for the D-Star network infrastructure.
It requires a 24/7 internet connection which can support 20 voice streams or more
to connect repeaters and hotspot dongles!!
 
 - The server requires a fix IP-address !
 - The public IP address should have a DNS record which must be published in the
   common host files.
 
If you want to run this software please make sure that you can provide this
service free of charge, like the developer team provides the software and the
network infrastructure free of charge!
Path file running xlxd are:
```
Exec /xlxd
Config file /xlxd
```
# Requirements

The software packages for Linux are tested on Debian7 (Wheezy) 32 and 64bit or newer.
Raspbian will work but is not recommended.
Please use the stable version listed above, we cannot support others.
 
# Installation

## Debian 7 and up 32 and 64bit
###### After a clean installation of debian make sure to run update and upgrade
```
# apt-get update
# apt-get upgrade
 ```
###### Install Git
```
# apt-get install git git-core
```
###### Install webserver with PHP5 support Debian 7 and 8
```
# apt-get install apache2 php5
```
###### Install webserver with PHP7 support Debian 9 (Stretch)
```
# apt-get install apache2 php
```
###### Install g++ compiler
```
For Debian 8 and 9
# apt-get install build-essential

For Debian 7
# apt-get install g++-4.7
```
###### Download and compile the XLX sources
 ```
# git clone https://github.com/LX3JL/xlxd.git
# cd xlxd/src/
# make clean
# make
# make install
 ```
###### Copy startup script "xlxd" to /etc/init.d
```
# cp ~/xlxd/Systemd/xlxd /etc/init.d/xlxd
```
###### Adapt the default startup parameters to your needs
```
# pico /etc/init.d/xlxd
```
###### Crontab the dmrid.dat from the XLXAPI server to your xlxd folder
```
Open file:
# nano /etc/crontab
 
Add at the bottom of the file
0 0    * * *   root    wget -O /xlxd/dmrid.dat http://xlxapi.rlx.lu/api/exportdmr.php
```
###### Last step is to declare the service for automatic startup and shutdown
```
For Debian 7
# update-rc.d xlxd defaults

For Debian 8 and up
# cp ~/xlxd/Systemd/xlxd.* /lib/systemd/system/
# systemctl daemon-reload
# systemctl enable xlxd.timer
```
###### Start or stop the service with
```
# service xlxd start
# service xlxd stop
# service xlxd restart
# service xlxd status
```
###### Copy dashboard to /var/www
```
Debian 7
# cp -r ~/xlxd/dashboard /var/www/db

Debian 8 and up
# cp -r ~/xlxd/dashboard /var/www/html/db
```
###### Give the dashboard read access to the server log file 
 ```
# chmod +r /var/log/messages 
 ```
###### Reboot server to see if the auto-start is working
 ```
# reboot
 ```
# Firewall settings #
 
XLX Server requires the following ports to be open and forwarded properly for in- and outgoing network traffic:
 - TCP port 80            (http) optional TCP port 443 (https)
 - TCP port 8080          (RepNet) optional
 - UDP port 10001         (json interface XLX Core)
 - UDP port 10002         (XLX interlink)
 - TCP port 22            (ssh) optional  TCP port 10022
 - UDP port 30001         (DExtra protocol)
 - UPD port 20001         (DPlus protocol)
 - UDP port 30051         (DCS protocol)
 - UDP port 8880          (DMR+ DMO mode)
 - UDP port 62030         (MMDVM protocol)
 - UDP port 10100         (AMBE controller port)
 - UDP port 10101 - 10199 (AMBE transcoding port)

© 2016 Luc Engelmann LX1IQ
