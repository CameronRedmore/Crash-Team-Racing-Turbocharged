#!/bin/bash
cd /tmp/b64 && make -k ctr_native > /tmp/b64.log 2>&1
grep -c "error:" /tmp/b64.log
grep "error:" /tmp/b64.log | sed -E 's/.*error: //' | sed -E "s/'[^']*'/X/g" | sort | uniq -c | sort -nr | head -${1:-15}
echo ---; grep "error:" /tmp/b64.log | sed -E 's/^.*64bit\///' | awk -F: '{print $1}' | sort | uniq -c | sort -nr | head -${2:-25}
