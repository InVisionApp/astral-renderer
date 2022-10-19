INSTALL_LOCATION_VALUE=$(shell $(ECHO) $(INSTALL_LOCATION))

FORCE:
.PHONY:FORCE

astral-release.pc: astral.pc.in FORCE
	@$(ECHO) Generating $@
	@cp $< $@
	@$(SED_INPLACE_REPLACE) 's!@COMPILE_FLAGS@!$(BASE_COMPILE_RELEASE_FLAGS)!g' $@
	@$(SED_INPLACE_REPLACE) 's!@LINK_FLAGS@!$(BASE_LINKER_RELEASE_FLAGS)!g' $@
	@$(SED_INPLACE_REPLACE) 's!@TYPE@!release!g' $@
	@$(SED_INPLACE_REPLACE) 's!@OTHER_TYPE@!debug!g' $@
	@$(SED_INPLACE_REPLACE) 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $@
CLEAN_FILES += astral-release.pc

astral-debug.pc: astral.pc.in FORCE
	@$(ECHO) Generating $@
	@cp $< $@
	@$(SED_INPLACE_REPLACE) 's!@COMPILE_FLAGS@!$(BASE_COMPILE_DEBUG_FLAGS)!g' $@
	@$(SED_INPLACE_REPLACE) 's!@LINK_FLAGS@!$(BASE_LINKER_DEBUG_FLAGS)!g' $@
	@$(SED_INPLACE_REPLACE) 's!@TYPE@!debug!g' $@
	@$(SED_INPLACE_REPLACE) 's!@OTHER_TYPE@!release!g' $@
	@$(SED_INPLACE_REPLACE) 's!@INSTALL_LOCATION@!$(INSTALL_LOCATION_VALUE)!g' $@
CLEAN_FILES += astral-debug.pc

ifeq ($(INSTALL_EXES),)
install_exes:
else
install_exes: $(INSTALL_EXES)
	-install $(INSTALL_EXES) $(INSTALL_LOCATION_VALUE)/bin
endif

install: astral-release.pc astral-debug.pc $(INSTALL_LIBS) $(INSTALL_EXES) install_exes
	-install -d $(INSTALL_LOCATION_VALUE)/lib
	-install -d $(INSTALL_LOCATION_VALUE)/lib/pkgconfig
	-install -d $(INSTALL_LOCATION_VALUE)/bin
	-install -d $(INSTALL_LOCATION_VALUE)/include
	-install $(INSTALL_LIBS) $(INSTALL_LOCATION_VALUE)/lib
	-install -m 666 astral-release.pc $(INSTALL_LOCATION_VALUE)/lib/pkgconfig
	-install -m 666 astral-debug.pc $(INSTALL_LOCATION_VALUE)/lib/pkgconfig
	-cd inc && find . -type d -exec echo {} \; | xargs -I '{}' install -d $(INSTALL_LOCATION_VALUE)/include/'{}'
	-cd inc && find . -type f -exec echo {} \; | xargs -I '{}' install -m 666 '{}' $(INSTALL_LOCATION_VALUE)/include/'{}'

uninstall:
	-rm -r $(INSTALL_LOCATION_VALUE)/lib/pkgconfig/astral-debug.pc
	-rm -r $(INSTALL_LOCATION_VALUE)/lib/pkgconfig/astral-release.pc
	-cd $(INSTALL_LOCATION_VALUE)/lib && rm $(INSTALL_LIBS)
	-rm -r $(INSTALL_LOCATION_VALUE)/include/astral
