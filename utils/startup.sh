#!/bin/sh

# Get tftp server from $siaddr
TFTP=$(cat /tftp)

busybox tftp -g -l ezio.sh -r ezio/ezio.sh $TFTP
sh ezio.sh

echo "Clone done."
sleep 5
poweroff -f
