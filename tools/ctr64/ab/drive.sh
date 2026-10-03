#!/bin/bash
# usage: BIN=/path/ctr_native GAMEARGS="..." TAG=name [GDB=1] drive.sh BOOTSECS STEP...
#   STEP = KEY | sleep:N | shot:NAME | hold:KEY:SECS | kb1 | wait  (wait = until the game exits, max 1800 s)
# S is a scratch dir holding config.ini.orig, debug/, memcards/, mods/, an assets symlink and inner.sh.
export S=${S:-/tmp/ctr64-ab}
cd $S
cp -f $BIN $S/ctr_native
cp -f config.ini.orig config.ini
sed -i 's/^borderless=.*/borderless=0/' config.ini
export SDL_VIDEO_DRIVER=x11 SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy
export TAG=${TAG:-run} GAMEARGS GDB
rm -f "$S/Crash Team Racing: Turbocharged.log"
xvfb-run -a -s "-screen 0 1280x720x24" $S/inner.sh "$@"
cp -f "$S/Crash Team Racing: Turbocharged.log" "$S/log_$TAG.txt" 2>/dev/null
true
