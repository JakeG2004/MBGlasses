# Meeting September 10, 2026

## Discussed how glasses work
* 802.15.4 network protocol
    * Ad-hoc protocol
    * ~250 kbps (fact check)
    * 2.4 ghz band
* 8 bit dipswitch
* currently uses 4 bits -> 16 channels
* 1 byte / color channel

## Discussed laptop
* USB permissions with latest ubuntu version
* broadcasts to all simultaneously

## Solidified our goal
* Use all 8 dip switches -> up to 256 channels
    * Make it so that each channel can be individually addressed
* Either reduce size of broadcast or add second antenna to get more throughput or both
* 10+ frames per sec of throughput
    * 200 bytes / 0.1 seconds = 2kbps
* Want 2 applications
    * Nikki friendly design application
    * Rinker friendly broadcast application

## Things to look into
* Drone software?
* MRF24340 (receiver microcontroller)
* ATMEGA328P (PCB microcontroller)

## Other
* Signed team contract

## Action items
* Get code from Rinker
* Start looking at code, network protocol, and microchips.