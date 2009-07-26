viaduct_config.h: Makefile
	echo '#define VIADUCT_PREFIX "$(prefix)"' >$@
#	git show --abbrev-commit | grep "^commit" | awk '{print "#define VIADUCT_BUILD \"" $$2 "\""}' >>$@
	echo "#define VIADUCT_BUILD \"@VIADUCT_BUILD@\"" >>$@

all-am: viaduct_config.h

