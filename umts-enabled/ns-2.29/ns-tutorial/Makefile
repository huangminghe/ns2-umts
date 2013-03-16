
nstutorial.tar.gz:
	rm -f nstutorial
	ln -s . nstutorial
	tar cvf - nstutorial/*.html nstutorial/*.txt \
		nstutorial/examples/*.tcl \
		nstutorial/examples/*.cc \
		nstutorial/examples/*.h \
		nstutorial/images/*.gif \
		nstutorial/Makefile | gzip >nstutorial.tar.gz


