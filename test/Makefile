# -*- Mode: makefile-gmake -*-

cleaner: clean
	@rm -f *~
	@rm -f common/*~
	@rm -f coverage/*.gcov
	@rm -fr coverage/results

all:
%:
	@$(MAKE) -C test_util $*
