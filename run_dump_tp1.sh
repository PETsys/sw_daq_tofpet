PREFIX=data/2014/05/27/M17M18/R001_fetp_tp1

mkdir -p $(dirname $PREFIX)
for ((CHANNEL=0; $CHANNEL < 64; CHANNEL=$[CHANNEL+6])); do
	P2=${PREFIX}_$(printf %02d $CHANNEL)
	python do_dump_tp1.py 0.1 $CHANNEL $P2
done
