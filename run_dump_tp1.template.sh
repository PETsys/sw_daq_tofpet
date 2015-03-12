PREFIX=data/YYYY/MM/DD/M1M2/R001_fetp1

mkdir -p $(dirname $PREFIX)
for ((CHANNEL=0; $CHANNEL < 64; CHANNEL=$[CHANNEL+6])); do
	P2=${PREFIX}_$(printf %02d $CHANNEL)
	python do_dump_tp1.py 0.1 $CHANNEL $P2
done
