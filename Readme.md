# Unet
Unified Networking for lobbies.

## Technical info
Internally, each service sends packets on 3 or more separate channels.

* Channel 0: Internal lobby control channel. All "internal" lobby data resides here. The transferred data are all encoded json objects.
* Channel 1: Relay channel. Used when clients want to send packets to clients that don't share a service. Same as general purpose data, except starts with the destination peer ID and desired channel.
* Channel 2 and up: General purpose channels.

These channels are entirely separate from the public Context `SendTo` API. There, channel 0 is transferred internally on channel 2, channel 1 on channel 3, etc.

## Quirks
Below I'm listing some fun quirks I found out.

* When hosting a lobby yourself, searching for lobbies will not result in your own lobby showing up in the lobby list on Galaxy, but it does on Steam.
* While Steam's reliable packets have a send limit of 1 MB (actually 1024 * 1024), Galaxy's reliable packets don't seem to have a limit at all. It does get slower the more MB you send though, even on a gigabit network. (In my tests, transfer rate is roughly around 2 MB/s)
