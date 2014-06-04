#!/bin/bash
path=data/2014/06/02/M21M22B

sipm_dc_0=48
sipm_dc_1=48

trap "exit" INT

while [ $sipm_dc_0 -ne 0 ] && [ $sipm_dc_1 -ne 0 ]
do
echo $sipm_dc_0
echo $sipm_dc_1
echo "Calling update config"

python Update_config2.py $sipm_dc_0 $sipm_dc_1
#$python do_dump_noise.py $path/R002_noise.root -e
#root -l ${path}/R002_noise.root draw_threshold_scan.C -e
python check_baseline.py $sipm_dc_0 $sipm_dc_1 > base.txt -e

while read line
do
#echo -e "$line\n"
IFS=" " read increment0 increment1 <<< $line
#echo $increment0
#echo $increment1

#sipm_dc_0=$(($sipm_dc_0 + $increment0))
#sipm_dc_1=$(($sipm_dc_1 + $increment1))
sipm_dc_0=$increment0
sipm_dc_1=$increment1


done < base.txt 

done