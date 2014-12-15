#!/bin/bash

for f in *.txt; do
	echo "unmasking channels on configuration: $f"
	sed -i -e "s/\(DAC_CHANNEL_MASK_CH[0-9]\+ = \)1/\10/" $f
	#grep "DAC_CHANNEL_MASK_CH[0-9]* = " $f
done
