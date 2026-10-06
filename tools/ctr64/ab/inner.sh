#!/bin/bash
R=$XDG_RUNTIME_DIR
LAUNCH=()
[ -n "$GDB" ] && LAUNCH=(gdb -batch -ex run -ex bt -ex quit --args)
bwrap --die-with-parent --dev-bind / / --tmpfs /dev/input --tmpfs /run/udev --tmpfs /dev/snd \
  --tmpfs $R/pulse --ro-bind /dev/null $R/pipewire-0 --ro-bind /dev/null $R/pipewire-0-manager \
  "${LAUNCH[@]}" $S/ctr_native $GAMEARGS > $S/stdout_$TAG.txt 2>&1 &
P=$!
sleep $1; shift
W=$(xdotool search --onlyvisible --name . 2>/dev/null | tail -1)
key() { xdotool windowfocus $W keydown $1; sleep 0.1; xdotool keyup $1; sleep 0.4; }
kblog() { cat "$S/Crash Team Racing: Turbocharged.log" $S/debug/reports/*/*/ctr-native.log 2>/dev/null | grep "Keyboard assigned" | tail -1; }
for st in "$@"; do
  kill -0 $P 2>/dev/null || { echo DIED; break; }
  case $st in
    sleep:*) sleep ${st#sleep:};;
    shot:*) import -window root $S/shot_${TAG}_${st#shot:}.png;;
    hold:*) k=${st#hold:}; xdotool windowfocus $W keydown ${k%%:*}; sleep ${k##*:}; xdotool keyup ${k%%:*};;
    kb1) for n in 1 2 3 4; do kblog | grep -q "player 1" && break; key F4; done;;
    menu) for n in $(seq 1 20); do import -window root $S/.menu.png; python3 $(dirname $0)/menu_visible.py $S/.menu.png && break; key Return; sleep 4; done; sleep 1;;
    wait|wait:*) n=${st#wait}; n=${n#:}; for i in $(seq 1 ${n:-1800}); do kill -0 $P 2>/dev/null || break; sleep 1; done;;
    *) key $st;;
  esac
done
kill -0 $P 2>/dev/null && echo ALIVE
# Ask the game to quit (SDL turns SIGTERM into a quit event) so stdout is flushed.
G=$(pgrep -n -f "^$S/ctr_native")
[ -n "$G" ] && kill -TERM $G 2>/dev/null
for i in $(seq 1 20); do kill -0 $P 2>/dev/null || break; sleep 0.5; done
kill $P 2>/dev/null; wait $P; echo exit=$?
