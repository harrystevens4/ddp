
# About

This is my attempt at a cross between SDP and mDNS.
It is designed to be call-and-response, where a discovery packet is sent on 255.255.255.255 and any devices available will respond directly to the sender's address.

## How to use

`devicediscover addresses` will get the hostname and address of all the devices it can find
`ddpd` is the daemon that handles requests

# Install

`make`
`sudo ./install`
`sudo systemctl enable --now ddp` (optional)
