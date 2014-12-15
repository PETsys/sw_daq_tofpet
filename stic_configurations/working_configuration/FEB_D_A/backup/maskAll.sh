#!/bin/bash

for f in *.txt; do
	echo "masking channels on configuration: $f"
	sed -i -e "s/\(DAC_CHANNEL_MASK_CH[0-9]\+ = \)0/\11/" $f
	grep "DAC_CHANNEL_MASK_CH[0-9]* = " $f
done
