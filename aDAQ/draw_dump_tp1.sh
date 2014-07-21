PREFIX=data/2014/07/21/M21M05/R001_fetp_tp1

for F in ${PREFIX}*.raw2; do 

	D=$(dirname $F)
	P=$(basename $F .raw2)
	echo $D $P
	mkdir -p ${D}/pdf
	mkdir -p ${D}/rpt

	aDAQ/buildRaw ${D}/${P} ${D}/rpt/${P}_rpt.root

	# Mezzanine on slot 0
	root -l -q ${D}/rpt/${P}_rpt.root aDAQ/setASIC0.C aDAQ/draw_dump_tp1.C
	cp str.txt ${D}/rpt/${P}_0_str.txt
	mv str.root ${D}/rpt/${P}_0_str.root

	cp ${D}/rpt/${P}_0_str.txt str.txt
 	root -l -q aDAQ/gDraw2D_tp1.C
 	mv graph2d.pdf ${D}/pdf/${P}_0_str.pdf

	# Mezzanine on slot 1
	root -l -q ${D}/rpt/${P}_rpt.root aDAQ/setASIC1.C aDAQ/draw_dump_tp1.C
	cp str.txt ${D}/rpt/${P}_1_str.txt
	mv str.root ${D}/rpt/${P}_1_str.root

	cp ${D}/rpt/${P}_1_str.txt str.txt
 	root -l -q aDAQ/gDraw2D_tp1.C
 	mv graph2d.pdf ${D}/pdf/${P}_1_str.pdf

done

