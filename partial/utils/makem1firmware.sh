#!/bin/sh

# "Compiles" firmware for Milestone1 on Octopoc++ on kea128

rm -f $2
# write magic nubmer
echo -n -e \\x89\\xfe\\xa0\\x13 >> $2
#for line in `cat $1`; do
while read line; do
	tokens=($line)
	if [ ${tokens[0]} = "LEDS" ]; then
	    echo -n -e "\x${tokens[1]}" >> $2
	    echo -n -e \\x00\\x00\\x00 >> $2
	elif [ ${tokens[0]} = "LAMPS" ]; then
	    echo -n -e "\x${tokens[1]}" >> $2
	    echo -n -e \\x00\\x00\\x01 >> $2
	elif [ ${tokens[0]} = "WAIT" ]; then
	    num=${tokens[1]}
	    hb=$(( (num / (256*256)) % 256 ))
	    mb=$(( (num / 256) % 256 ))
	    lb=$(( num % 256 ))
	    echo -n -e $(printf '\\x%02x\\x%02x\\x%02x' $lb $mb $hb) >> $2
	    echo -n -e \\x02 >> $2
	elif [ ${tokens[0]} = "LOOP" ]; then
	    echo -n -e \\x00\\x00\\x00\\x03 >> $2
	fi
done <$1

