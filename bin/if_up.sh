#!/bin/sh
sudo ifconfig tap0 10.0.2.22
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -A FORWARD -o tap0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i tap0 ! -o tap0 -j ACCEPT
sudo iptables -A POSTROUTING -s 10.0.0.0/16 ! -o tap0 -j MASQUERADE -t nat
