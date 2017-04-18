rm -f /log.txt
killall opdrc
nice -n -17 /bin/opdrc > /log.txt &