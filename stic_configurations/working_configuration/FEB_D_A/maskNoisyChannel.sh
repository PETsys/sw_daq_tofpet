#!/bin/bash

#echo "masking channel on configuration: "

#FEB-A B234384
sed -i -e "s/\(DAC_CHANNEL_MASK_CH54 = \)0/\11/" working_configuration_chip_4.txt

#FEB-A B233956
sed -i -e "s/\(DAC_CHANNEL_MASK_CH26 = \)0/\11/" working_configuration_chip_7.txt
