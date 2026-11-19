#!/bin/bash
# Checks that every official link in data/schemes.txt opens.
# Needs an internet connection. Run it once before the demo:  make links
#
# 200/301/302 = fine. Some government sites refuse scripts (403/000): open those in the browser to confirm.

grep -E "^(link|apply) = " data/schemes.txt | sed 's/^[a-z]* = //' | sort -u | while read -r url; do
    code=$(curl -s -o /dev/null -L -m 20 -A "Mozilla/5.0" -w "%{http_code}" "$url")
    if [[ "$code" == 2* || "$code" == 3* ]]; then
        echo "OK    $code  $url"
    else
        echo "CHECK $code  $url"
    fi
done
