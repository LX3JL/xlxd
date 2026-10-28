#!/bin/bash

# Reset UFW to start with a clean slate
sudo ufw reset

# Set default policies
sudo ufw default deny incoming
sudo ufw default allow outgoing

# Allow specific ports and protocols
# HTTP and HTTPS
sudo ufw allow 80/tcp        # HTTP
sudo ufw allow 443/tcp       # HTTPS (optional)

# Additional TCP Ports
sudo ufw allow 8080/tcp      # Additional HTTP service
sudo ufw allow 22/tcp        # SSH

# UDP Ports
sudo ufw allow 10001/udp     # UDP port 10001
sudo ufw allow 10002/udp     # UDP port 10002
sudo ufw allow 42000/udp     # UDP port 42000
sudo ufw allow 30001/udp     # UDP port 30001
sudo ufw allow 20001/udp     # UDP port 20001
sudo ufw allow 30051/udp     # UDP port 30051
sudo ufw allow 62030/udp     # UDP port 62030
sudo ufw allow 8880/udp      # UDP port 8880
sudo ufw allow 10100/udp     # UDP port 10100

# Port Ranges
sudo ufw allow 10101:10199/udp   # UDP ports 10101-10199
sudo ufw allow 12345:12346/udp   # UDP ports 12345-12346
sudo ufw allow 40000/udp         # UDP port 40000
sudo ufw allow 21110/udp         # UDP port 21110

# Enable UFW
sudo ufw enable

# Show UFW status
sudo ufw status verbose