#!/bin/sh
sudo iptables -D FORWARD -o tap0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -D FORWARD -i tap0 ! -o tap0 -j ACCEPT
sudo iptables -D POSTROUTING -s 10.0.0.0/16 ! -o tap0 -j MASQUERADE -t nat
