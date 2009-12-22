dbrelay_config.h: Makefile
	echo '#define DBRELAY_PREFIX "$(prefix)"' >$@
#	git show --abbrev-commit | grep "^commit" | awk '{print "#define DBRELAY_BUILD \"" $$2 "\""}' >>$@
	echo "#define DBRELAY_BUILD \"@DBRELAY_BUILD@\"" >>$@
	echo "#define DBRELAY_MAGIC \"@DBRELAY_MAGIC@\"" >>$@

all-am: dbrelay_config.h

