
ICUDATA_PLATFORM_NAME=icudt$(U_MAJOR_VERSION)$(ICUDATA_CHAR)
ICUDATA_ENTRY_POINT=icudt$(U_MAJOR_VERSION)

SRCDATADIR=$(top_srcdir)/data
OUTDIR=$(top_builddir)/data/out

OUTTMPDIR=$(OUTDIR)/tmp
MAINBUILDDIR=$(OUTDIR)/build
BUILDDIR=$(MAINBUILDDIR)/$(ICUDATA_PLATFORM_NAME)
UNICODEDATADIR=$(SRCDATADIR)/unidata
LOCSRCDIR=$(SRCDATADIR)/locales
CURRSRCDIR=$(SRCDATADIR)/curr
CURRBLDDIR=$(BUILDDIR)/curr
LANGSRCDIR=$(SRCDATADIR)/lang
LANGBLDDIR=$(BUILDDIR)/lang
REGIONSRCDIR=$(SRCDATADIR)/region
REGIONBLDDIR=$(BUILDDIR)/region
ZONESRCDIR=$(SRCDATADIR)/zone
ZONEBLDDIR=$(BUILDDIR)/zone
COLSRCDIR=$(SRCDATADIR)/coll
COLBLDDIR=$(BUILDDIR)/coll
RBNFSRCDIR=$(SRCDATADIR)/rbnf
RBNFBLDDIR=$(BUILDDIR)/rbnf
TRANSLITSRCDIR=$(SRCDATADIR)/translit
TRANSLITBLDDIR=$(BUILDDIR)/translit
MISCSRCDIR=$(SRCDATADIR)/misc
BRKSRCDIR=$(SRCDATADIR)/brkitr
BRKBLDDIR=$(BUILDDIR)/brkitr
UCMSRCDIR=$(SRCDATADIR)/mappings
SPREPSRCDIR=$(SRCDATADIR)/sprep
COMINCDIR=$(top_srcdir)/common/unicode
SRCLISTDEPS=Makefile $(srcdir)/Makefile.in $(srcdir)/Makefile.am
BUILD_DIRS=$(OUTDIR) $(MAINBUILDDIR) $(BUILDDIR) $(CURRBLDDIR) $(LANGBLDDIR) $(REGIONBLDDIR) $(ZONEBLDDIR) $(BRKBLDDIR) $(COLBLDDIR) $(RBNFBLDDIR) $(TRANSLITBLDDIR) $(OUTTMPDIR) $(OUTTMPDIR_390STUB) $(OUTTMPDIR)/$(CURR_TREE) $(OUTTMPDIR)/$(LANG_TREE) $(OUTTMPDIR)/$(REGION_TREE) $(OUTTMPDIR)/$(ZONE_TREE) $(OUTTMPDIR)/$(COLLATION_TREE) $(OUTTMPDIR)/$(RBNF_TREE) $(OUTTMPDIR)/$(TRANSLIT_TREE) $(OUTTMPDIR)/$(BREAK_TREE)

TOOLDIR=$(top_builddir)/tools
ICUPKG=$(TOOLDIR)/icupkg/icupkg$(EXEEXT)
GENCNVAL=$(TOOLDIR)/gencnval/gencnval$(EXEEXT)
MAKECONV=$(TOOLDIR)/makeconv/makeconv$(EXEEXT)
GENBRK=$(TOOLDIR)/genbrk/genbrk$(EXEEXT)
GENSPREP=$(TOOLDIR)/gensprep/gensprep$(EXEEXT)
GENDICT=$(TOOLDIR)/gendict/gendict$(EXEEXT)
GENCFU=$(TOOLDIR)/gencfu/gencfu$(EXEEXT)
GENRB=$(TOOLDIR)/genrb/genrb$(EXEEXT)
PKGDATA=$(TOOLDIR)/pkgdata/pkgdata$(EXEEXT)

##### Define all the data files. the build rule that depends on them is below.
# X_FILES_SHORT = just the base names (for lists)
# X_FILES = full paths (for dependency)

## DAT files - Misc. data files.
#  2005-may-05 Removed Unicode properties files (unorm.icu, uprops.icu, ucase.icu, ubidi.icu)
#  from data build. See Jitterbug 4497. (makedata.mak revision 1.117)
#  2010-dec Removed pnames.icu.
# These are now hardcoded in ICU4C and only loaded in ICU4J.
#
DAT_FILES_SHORT=unames.icu cnvalias.icu coll/ucadata.icu nfc.nrm nfkc.nrm nfkc_cf.nrm uts46.nrm
DAT_FILES=$(DAT_FILES_SHORT:%=$(BUILDDIR)/%)

## BRK files
BREAK_TREE=brkitr
include $(BRKSRCDIR)/brkfiles.mk
ALL_BRK_SOURCE= $(BRK_SOURCE) $(BRK_SOURCE_LOCAL)
BRK_FILES_SHORT=$(ALL_BRK_SOURCE:%.txt=$(BREAK_TREE)/%.brk)
BRK_FILES=$(ALL_BRK_SOURCE:%.txt=$(BRKBLDDIR)/%.brk)
#ifdef BRK_DICT_SOURCE
ALL_DICT_SOURCE=$(BRK_DICT_SOURCE) $(BRK_DICT_SOURCE_LOCAL)
DICT_FILES_SHORT=$(ALL_DICT_SOURCE:%.txt=$(BREAK_TREE)/%.dict)
DICT_FILES=$(ALL_DICT_SOURCE:%.txt=$(BRKBLDDIR)/%.dict)
#endif
#ifdef BRK_RES_SOURCE
BRS_SRC= root.txt $(BRK_RES_SOURCE) $(BRK_RES_SOURCE_LOCAL)
BRS_SRC_FILES = $(BRS_SRC:%=$(BRKSRCDIR)/%)
INSTALLED_BRS_FILES = $(BRK_RES_SOURCE:%.txt=%)  $(BRK_RES_SOURCE_LOCAL:%.txt=%)
#endif

## Confusables (Spoofing) files
ALL_CFU_SOURCE=$(UNICODEDATADIR)/confusables.txt $(UNICODEDATADIR)/confusablesWholeScript.txt
CFU_FILES_SHORT=confusables.cfu
CFU_FILES=$(BUILDDIR)/$(CFU_FILES_SHORT)

## UCM files
include $(UCMSRCDIR)/ucmcore.mk
include $(UCMSRCDIR)/ucmfiles.mk
include $(UCMSRCDIR)/ucmebcdic.mk
ALL_UCM_SOURCE = ibm-37_P100-1995.ucm ibm-1047_P100-1995.ucm $(UCM_SOURCE_CORE) $(UCM_SOURCE_FILES) $(UCM_SOURCE_EBCDIC) $(UCM_SOURCE_LOCAL)
UCM_FILES = $(ALL_UCM_SOURCE:%=$(SRCDATADIR)/%)
CNV_FILES = $(ALL_UCM_SOURCE:%.ucm=$(BUILDDIR)/%.cnv)
CNV_FILES_SHORT = $(ALL_UCM_SOURCE:%.ucm=%.cnv)
UCM_SOURCE_SPECIAL=$(UCM_SOURCE_EBCDIC_IGNORE_SISO) $(UCM_SOURCE_EBCDIC_IGNORE_SISO_LOCAL)
UCM_FILES_SPECIAL=$(UCM_SOURCE_SPECIAL:%=$(UCMSRCDIR)/%) 
CNV_FILES_SPECIAL=$(UCM_SOURCE_SPECIAL:%.ucm=$(BUILDDIR)/%.cnv) 
CNV_FILES_SHORT_SPECIAL=$(UCM_SOURCE_SPECIAL:%.ucm=%.cnv) 

## RES files
include $(LOCSRCDIR)/resfiles.mk
include $(CURRSRCDIR)/resfiles.mk
include $(LANGSRCDIR)/resfiles.mk
include $(REGIONSRCDIR)/resfiles.mk
include $(ZONESRCDIR)/resfiles.mk
include $(COLSRCDIR)/colfiles.mk
include $(RBNFSRCDIR)/rbnffiles.mk
include $(TRANSLITSRCDIR)/trnsfiles.mk
-include $(LOCSRCDIR)/reslocal.mk
-include $(CURRSRCDIR)/reslocal.mk
-include $(LANGSRCDIR)/reslocal.mk
-include $(REGIONSRCDIR)/reslocal.mk
-include $(ZONESRCDIR)/reslocal.mk
-include $(COLSRCDIR)/collocal.mk
-include $(BRKSRCDIR)/brslocal.mk
-include $(RBNFSRCDIR)/rbnflocal.mk
-include $(TRANSLITSRCDIR)/trnslocal.mk
#ifdef GENRB_SOURCE
RES_SRC= root.txt $(GENRB_SOURCE) $(GENRB_ALIAS_SOURCE) $(GENRB_SOURCE_LOCAL) $(GENRB_ALIAS_SOURCE_LOCAL)
RES_SRC_FILES = $(RES_SRC:%=$(LOCSRCDIR)/%)
INSTALLED_RB_FILES = $(GENRB_SOURCE:%.txt=%) $(GENRB_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef CURR_SOURCE
CURR_SRC= root.txt supplementalData.txt $(CURR_SOURCE) $(CURR_ALIAS_SOURCE) $(CURR_SOURCE_LOCAL)
CURR_SRC_FILES = $(CURR_SRC:%=$(CURRSRCDIR)/%)
INSTALLED_CURR_FILES = $(CURR_SOURCE:%.txt=%)  $(CURR_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef LANG_SOURCE
LANG_SRC= root.txt $(LANG_SOURCE) $(LANG_ALIAS_SOURCE) $(LANG_SOURCE_LOCAL)
LANG_SRC_FILES = $(LANG_SRC:%=$(LANGSRCDIR)/%)
INSTALLED_LANG_FILES = $(LANG_SOURCE:%.txt=%)  $(LANG_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef REGION_SOURCE
REGION_SRC= root.txt $(REGION_SOURCE) $(REGION_ALIAS_SOURCE) $(REGION_SOURCE_LOCAL)
REGION_SRC_FILES = $(REGION_SRC:%=$(REGIONSRCDIR)/%)
INSTALLED_REGION_FILES = $(REGION_SOURCE:%.txt=%)  $(REGION_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef ZONE_SOURCE
ZONE_SRC= root.txt $(ZONE_SOURCE) $(ZONE_ALIAS_SOURCE) $(ZONE_SOURCE_LOCAL)
ZONE_SRC_FILES = $(ZONE_SRC:%=$(ZONESRCDIR)/%)
INSTALLED_ZONE_FILES = $(ZONE_SOURCE:%.txt=%)  $(ZONE_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef COLLATION_SOURCE
COL_SRC= root.txt $(COLLATION_SOURCE) $(COLLATION_ALIAS_SOURCE) $(COLLATION_SOURCE_LOCAL)
COL_SRC_FILES = $(COL_SRC:%=$(COLSRCDIR)/%)
INSTALLED_COL_FILES = $(COLLATION_SOURCE:%.txt=%)  $(COLLATION_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef RBNF_SOURCE
RBNF_SRC= root.txt $(RBNF_SOURCE) $(RBNF_ALIAS_SOURCE) $(RBNF_SOURCE_LOCAL)
RBNF_SRC_FILES = $(RBNF_SRC:%=$(RBNFSRCDIR)/%)
INSTALLED_RBNF_FILES = $(RBNF_SOURCE:%.txt=%)  $(RBNF_SOURCE_LOCAL:%.txt=%)
#endif
#ifdef TRANSLIT_SOURCE
TRANSLIT_SRC= $(TRANSLIT_SOURCE) $(TRANSLIT_ALIAS_SOURCE) $(TRANSLIT_SOURCE_LOCAL)
TRANSLIT_SRC_FILES = $(TRANSLIT_SRC:%=$(TRANSLITSRCDIR)/%)
INSTALLED_TRANSLIT_FILES = $(TRANSLIT_SOURCE:%.txt=%)  $(TRANSLIT_SOURCE_LOCAL:%.txt=%)
#endif
GENRBOPTS=-k

## MISC files
include $(MISCSRCDIR)/miscfiles.mk
-include $(MISCSRCDIR)/misclocal.mk
MSC_SOURCE= $(MISC_SOURCE) $(MISC_SOURCE_LOCAL)
MSC_SRC_FILES=$(MSC_SOURCE:%=$(MISCSRCDIR)/%)

INDEX_NAME=res_index
INDEX_FILE=$(OUTTMPDIR)/$(INDEX_NAME).txt

ALL_RES_SRC= $(RES_SRC) $(TRNS_SOURCE) $(MSC_SOURCE)
RES_FILES = $(ALL_RES_SRC:%.txt=$(BUILDDIR)/%.res) $(BUILDDIR)/$(INDEX_NAME).res $(BUILDDIR)/pool.res
RES_FILES_SHORT = $(ALL_RES_SRC:%.txt=%.res) $(INDEX_NAME).res pool.res
PKGDATA_LIST = $(OUTTMPDIR)/icudata.lst

CURR_TREE=curr
CURR_INDEX_FILE=$(OUTTMPDIR)/$(CURR_TREE)/$(INDEX_NAME).txt
CURR_FILES = $(CURR_SRC:%.txt=$(CURRBLDDIR)/%.res) $(CURRBLDDIR)/$(INDEX_NAME).res $(CURRBLDDIR)/pool.res
CURR_FILES_SHORT = $(CURR_SRC:%.txt=$(CURR_TREE)/%.res) $(CURR_TREE)/$(INDEX_NAME).res $(CURR_TREE)/pool.res

LANG_TREE=lang
LANG_INDEX_FILE=$(OUTTMPDIR)/$(LANG_TREE)/$(INDEX_NAME).txt
LANG_FILES = $(LANG_SRC:%.txt=$(LANGBLDDIR)/%.res) $(LANGBLDDIR)/$(INDEX_NAME).res $(LANGBLDDIR)/pool.res
LANG_FILES_SHORT = $(LANG_SRC:%.txt=$(LANG_TREE)/%.res) $(LANG_TREE)/$(INDEX_NAME).res $(LANG_TREE)/pool.res

REGION_TREE=region
REGION_INDEX_FILE=$(OUTTMPDIR)/$(REGION_TREE)/$(INDEX_NAME).txt
REGION_FILES = $(REGION_SRC:%.txt=$(REGIONBLDDIR)/%.res) $(REGIONBLDDIR)/$(INDEX_NAME).res $(REGIONBLDDIR)/pool.res
REGION_FILES_SHORT = $(REGION_SRC:%.txt=$(REGION_TREE)/%.res) $(REGION_TREE)/$(INDEX_NAME).res $(REGION_TREE)/pool.res

ZONE_TREE=zone
ZONE_INDEX_FILE=$(OUTTMPDIR)/$(ZONE_TREE)/$(INDEX_NAME).txt
ZONE_FILES = $(ZONE_SRC:%.txt=$(ZONEBLDDIR)/%.res) $(ZONEBLDDIR)/$(INDEX_NAME).res $(ZONEBLDDIR)/pool.res
ZONE_FILES_SHORT = $(ZONE_SRC:%.txt=$(ZONE_TREE)/%.res) $(ZONE_TREE)/$(INDEX_NAME).res $(ZONE_TREE)/pool.res

COLLATION_TREE=coll
COLLATION_INDEX_FILE=$(OUTTMPDIR)/$(COLLATION_TREE)/$(INDEX_NAME).txt
COLLATION_INDEX_RES=$(COLBLDDIR)/$(INDEX_NAME).res
COLLATION_INDEX_RES_SHORT=$(COLLATION_TREE)/$(INDEX_NAME).res
COLLATION_FILES = $(COL_SRC:%.txt=$(COLBLDDIR)/%.res) $(COLLATION_INDEX_RES)
COLLATION_FILES_SHORT = $(COL_SRC:%.txt=$(COLLATION_TREE)/%.res)

BRK_RES_INDEX_FILE=$(OUTTMPDIR)/$(BREAK_TREE)/$(INDEX_NAME).txt
BRK_RES_INDEX_RES=$(BRKBLDDIR)/$(INDEX_NAME).res
BRK_RES_INDEX_RES_SHORT=$(BREAK_TREE)/$(INDEX_NAME).res
BRK_RES_FILES = $(BRS_SRC:%.txt=$(BRKBLDDIR)/%.res) $(BRK_RES_INDEX_RES)
BRK_RES_FILES_SHORT = $(BRS_SRC:%.txt=$(BREAK_TREE)/%.res)

RBNF_TREE=rbnf
RBNF_INDEX_FILE=$(OUTTMPDIR)/$(RBNF_TREE)/$(INDEX_NAME).txt
RBNF_INDEX_RES=$(RBNFBLDDIR)/$(INDEX_NAME).res
RBNF_INDEX_RES_SHORT=$(RBNF_TREE)/$(INDEX_NAME).res
RBNF_FILES = $(RBNF_SRC:%.txt=$(RBNFBLDDIR)/%.res) $(RBNF_INDEX_RES)
RBNF_FILES_SHORT = $(RBNF_SRC:%.txt=$(RBNF_TREE)/%.res)

TRANSLIT_TREE=translit
#TRANSLIT_INDEX_FILE=$(OUTTMPDIR)/$(TRANSLIT_TREE)/$(INDEX_NAME).txt
#TRANSLIT_INDEX_RES=$(TRANSLITBLDDIR)/$(INDEX_NAME).res
#TRANSLIT_INDEX_RES_SHORT=$(TRANSLIT_TREE)/$(INDEX_NAME).res
TRANSLIT_FILES = $(TRANSLIT_SRC:%.txt=$(TRANSLITBLDDIR)/%.res)
TRANSLIT_FILES_SHORT = $(TRANSLIT_SRC:%.txt=$(TRANSLIT_TREE)/%.res)

## SPP files
include $(SPREPSRCDIR)/sprepfiles.mk
-include $(SPREPSRCDIR)/spreplocal.mk
ALL_SPREP_SOURCE=$(SPREP_SOURCE) $(SPREP_SOURCE_LOCAL)
SPREP_FILES = $(ALL_SPREP_SOURCE:%.txt=$(BUILDDIR)/%.spp)
SPREP_FILES_SHORT = $(ALL_SPREP_SOURCE:%.txt=%.spp)

## All generated files
ALL_FILES=$(DAT_FILES) $(CNV_FILES) $(CNV_FILES_SPECIAL) $(BRK_FILES) $(DICT_FILES) $(RES_FILES) $(INDEX_RES_FILE) $(CURR_FILES) $(LANG_FILES) $(REGION_FILES) $(ZONE_FILES) $(COLLATION_FILES) $(BRK_RES_FILES) $(RBNF_FILES) $(TRANSLIT_FILES) $(SPREP_FILES) $(CFU_FILES) $(PKGDATA_LIST)
# a list to use in the .lst files (package-relative)
COLL_FILES_LIST=$(COLLATION_FILES_SHORT) $(COLLATION_INDEX_RES_SHORT)
BRK_FILES_LIST=$(BRK_FILES_SHORT) $(BRK_RES_FILES_SHORT) $(BRK_RES_INDEX_RES_SHORT) $(DICT_FILES_SHORT)
LOCALE_FILES_LIST= $(RES_FILES_SHORT) $(LANG_FILES_SHORT) $(REGION_FILES_SHORT) $(ZONE_FILES_SHORT)
MISC_FILES_LIST=$(DAT_FILES_SHORT) $(CNV_FILES_SHORT) $(CNV_FILES_SHORT_SPECIAL) $(CURR_FILES_SHORT) $(RBNF_FILES_SHORT) $(RBNF_INDEX_RES_SHORT) $(TRANSLIT_FILES_SHORT) $(SPREP_FILES_SHORT) $(CFU_FILES_SHORT)

build-dirs:
	@mkdir -p $(BUILD_DIRS)

icu-data-files: $(ALL_FILES)

packagedatadir = $(datadir)
packagedata_DATA = $(OUTDIR)/$(ICUDATA_PLATFORM_NAME).dat

$(OUTDIR)/$(ICUDATA_PLATFORM_NAME).dat:
	$(PKGDATA) -q -c -s $(BUILDDIR) -d $(OUTDIR) -e $(ICUDATA_ENTRY_POINT) -T $(OUTTMPDIR) -p $(ICUDATA_PLATFORM_NAME) -m common $(PKGDATA_LIST)

.PHONY: build-dirs icu-data-files

BUILT_SOURCES=build-dirs icu-data-files

clean-local:
	@-rm -fr $(OUTDIR)

#####################################################
# General data build rules

$(PKGDATA_LIST): $(SRCLISTDEPS)
	@echo "generating $@ (list of data files)"
	@-rm -f $@
	@for file in $(COLL_FILES_LIST); do \
	  echo $$file >> $@; \
	done;
	@for file in $(BRK_FILES_LIST); do \
	  echo $$file >> $@; \
	done;
	@for file in $(LOCALE_FILES_LIST); do \
	  echo $$file >> $@; \
	done;
	@for file in $(MISC_FILES_LIST); do \
	  echo $$file >> $@; \
	done;

# Now, sections for building each kind of data.

####################################################    DAT
# DAT FILES

# cnvalias.icu
$(BUILDDIR)/cnvalias.icu: $(UCMSRCDIR)/convrtrs.txt
	$(GENCNVAL) -d $(BUILDDIR) $(UCMSRCDIR)/convrtrs.txt

# Targets for prebuilt Unicode data
$(BUILDDIR)/%.icu: $(SRCDATADIR)/in/%.icu
	$(ICUPKG) -t$(ICUDATA_CHAR) $< $@

$(BUILDDIR)/%.nrm: $(SRCDATADIR)/in/%.nrm
	$(ICUPKG) -t$(ICUDATA_CHAR) $< $@

$(BUILDDIR)/coll/%.icu: $(SRCDATADIR)/in/coll/%.icu
	$(ICUPKG) -t$(ICUDATA_CHAR) $< $@

####################################################    SPP
# SPP FILES

$(BUILDDIR)/%.spp: $(SPREPSRCDIR)/%.txt $(BUILDDIR)/unames.icu
	$(GENSPREP) -d $(BUILDDIR) -i $(BUILDDIR) -s $(SPREPSRCDIR) -b $(@F:%.spp=%) -m $(UNICODEDATADIR) -u 3.2.0 $(<F)


####################################################    BRK
# BRK FILES

$(BRKBLDDIR)/%.brk: $(BRKSRCDIR)/%.txt $(DAT_FILES)
	$(GENBRK) -c -i $(BUILDDIR) -r $< -o $@

####################################################    DICT
# DICT FILES

# .dict file generated regardless of whether dictionary file exists

$(BRKBLDDIR)/%.dict: $(DAT_FILES)
	$(GENDICT) --uchars -c -i $(BUILDDIR) $(BRKSRCDIR)/$(*F).txt $@

$(BRKBLDDIR)/thaidict.dict: $(DAT_FILES)
	$(GENDICT) --bytes --transform offset-0x0e00 -c -i $(BUILDDIR) $(BRKSRCDIR)/thaidict.txt $(BRKBLDDIR)/thaidict.dict

$(BRKBLDDIR)/laodict.dict: $(DAT_FILES)
	$(GENDICT) --bytes --transform offset-0x0e80 -c -i $(BUILDDIR) $(BRKSRCDIR)/laodict.txt $(BRKBLDDIR)/laodict.dict

# TODO: figure out why combining characters are here?
$(BRKBLDDIR)/khmerdict.dict: $(DAT_FILES)
	$(GENDICT) --bytes --transform offset-0x1780 -c -i $(BUILDDIR) $(BRKSRCDIR)/khmerdict.txt $(BRKBLDDIR)/khmerdict.dict

####################################################    CFU
# CFU FILES
#    Note: gencfu requires two input files to produce a single output file.
#          There will be exactly one target file and two source files.
#          The $(word n, ...) selects the nth word from the following stuff.
#          There must be a nicer way to do this.

$(CFU_FILES): $(ALL_CFU_SOURCE) $(DAT_FILES)
	@echo ALL_CFU_SOURCE: $(ALL_CFU_SOURCE)
	@echo CFU_FILES: $(CFU_FILES)
	@echo CFU_FILES_SHORT: $(CFU_FILES_SHORT)
	$(GENCFU) -c -i $(BUILDDIR) -r $(word 1,$(ALL_CFU_SOURCE)) -w $(word 2,$(ALL_CFU_SOURCE)) -o $@


####################################################    CNV
# CNV FILES

$(CNV_FILES_SPECIAL): $(UCM_FILES_SPECIAL)
	$(MAKECONV) --ignore-siso-check -c -d $(BUILDDIR) $(UCMSRCDIR)/$(@F:%.cnv=%.ucm) 
$(BUILDDIR)/%.cnv: $(UCMSRCDIR)/%.ucm
	$(MAKECONV) -c -d $(BUILDDIR) $<

####################################################    RES
# RES FILES

### curr res
$(CURRBLDDIR)/%.res: $(CURRSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) --usePoolBundle $(GENRBOPTS) -i $(BUILDDIR) -s $(CURRSRCDIR) -d $(CURRBLDDIR) $(<F)

# copy the curr/pool.res file from the source folder to the build output folder
# and swap it to native endianness
$(CURRBLDDIR)/pool.res: $(CURRSRCDIR)/pool.res
	$(ICUPKG) -t$(ICUDATA_CHAR) $(CURRSRCDIR)/pool.res $(CURRBLDDIR)/pool.res

$(CURRBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(CURR_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(CURR_TREE) -d $(CURRBLDDIR) $(INDEX_NAME).txt

$(CURR_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed currency name locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_CURR_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### lang res
$(LANGBLDDIR)/%.res: $(LANGSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) --usePoolBundle $(GENRBOPTS) -i $(BUILDDIR) -s $(LANGSRCDIR) -d $(LANGBLDDIR) $(<F)

# copy the lang/pool.res file from the source folder to the build output folder
# and swap it to native endianness
$(LANGBLDDIR)/pool.res: $(LANGSRCDIR)/pool.res
	$(ICUPKG) -t$(ICUDATA_CHAR) $(LANGSRCDIR)/pool.res $(LANGBLDDIR)/pool.res

$(LANGBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(LANG_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(LANG_TREE) -d $(LANGBLDDIR) $(INDEX_NAME).txt

$(LANG_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed language name locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_LANG_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### region res
$(REGIONBLDDIR)/%.res: $(REGIONSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) --usePoolBundle $(GENRBOPTS) -i $(BUILDDIR) -s $(REGIONSRCDIR) -d $(REGIONBLDDIR) $(<F)

# copy the region/pool.res file from the source folder to the build output folder
# and swap it to native endianness
$(REGIONBLDDIR)/pool.res: $(REGIONSRCDIR)/pool.res
	$(ICUPKG) -t$(ICUDATA_CHAR) $(REGIONSRCDIR)/pool.res $(REGIONBLDDIR)/pool.res

$(REGIONBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(REGION_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(REGION_TREE) -d $(REGIONBLDDIR) $(INDEX_NAME).txt

$(REGION_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed region name locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_REGION_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### zone res
$(ZONEBLDDIR)/%.res: $(ZONESRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) --usePoolBundle $(GENRBOPTS) -i $(BUILDDIR) -s $(ZONESRCDIR) -d $(ZONEBLDDIR) $(<F)

# copy the zone/pool.res file from the source folder to the build output folder
# and swap it to native endianness
$(ZONEBLDDIR)/pool.res: $(ZONESRCDIR)/pool.res
	$(ICUPKG) -t$(ICUDATA_CHAR) $(ZONESRCDIR)/pool.res $(ZONEBLDDIR)/pool.res

$(ZONEBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(ZONE_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(ZONE_TREE) -d $(ZONEBLDDIR) $(INDEX_NAME).txt

$(ZONE_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed time zone name locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_ZONE_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### collation res
$(COLBLDDIR)/%.res: $(COLSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(COLSRCDIR) -d $(COLBLDDIR) $(<F)

$(COLBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(COLLATION_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(COLLATION_TREE) -d $(COLBLDDIR) $(INDEX_NAME).txt

$(COLLATION_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed collation locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_COL_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### brk res
$(BRKBLDDIR)/%.res: $(BRKSRCDIR)/%.txt $(BRK_FILES) $(DICT_FILES) $(DAT_FILES)
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(BRKSRCDIR) -d $(BRKBLDDIR) $(<F)

$(BRKBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(BREAK_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(BREAK_TREE) -d $(BRKBLDDIR) $(INDEX_NAME).txt

$(BRK_RES_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed break locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_BRS_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### RBNF res
$(RBNFBLDDIR)/%.res: $(RBNFSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(RBNFSRCDIR) -d $(RBNFBLDDIR) $(<F)

$(RBNFBLDDIR)/$(INDEX_NAME).res: $(OUTTMPDIR)/$(RBNF_TREE)/$(INDEX_NAME).txt
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(OUTTMPDIR)/$(RBNF_TREE) -d $(RBNFBLDDIR) $(INDEX_NAME).txt

$(RBNF_INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed RBNF locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_RBNF_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

### TRANSLIT res
$(TRANSLITBLDDIR)/%.res: $(TRANSLITSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -s $(TRANSLITSRCDIR) -d $(TRANSLITBLDDIR) $(<F)

$(BUILDDIR)/%.res: $(LOCSRCDIR)/%.txt $(DAT_FILES)
	$(GENRB) --usePoolBundle $(GENRBOPTS) -i $(BUILDDIR) -s $(LOCSRCDIR) -d $(BUILDDIR) $(<F)

# copy the locales/pool.res file from the source folder to the build output folder
# and swap it to native endianness
$(BUILDDIR)/pool.res: $(LOCSRCDIR)/pool.res
	$(ICUPKG) -t$(ICUDATA_CHAR) $(LOCSRCDIR)/pool.res $(BUILDDIR)/pool.res

# zoneinfo has some issues. Ignore some warnings with -q
$(BUILDDIR)/%.res: $(MISCSRCDIR)/%.txt
	$(GENRB) $(GENRBOPTS) -q -i $(BUILDDIR) -s $(MISCSRCDIR) -d $(BUILDDIR) $(<F)



$(INDEX_FILE): $(SRCLISTDEPS)
	@echo "generating $@ (list of installed locales)";
	@-rm -f $@;
	@echo "// Warning this file is automatically generated" > $@; \
	echo "$(INDEX_NAME):table(nofallback) {" >> $@; \
        echo "    CLDRVersion { \"$(GENRB_CLDR_VERSION)\" }" >> $@; \
	echo "    InstalledLocales {" >> $@; \
	for file in $(INSTALLED_RB_FILES); do \
	  echo "        $$file {\"\"}" >> $@; \
	done; \
	echo "    }" >> $@; \
	echo "}" >> $@;

$(BUILDDIR)/$(INDEX_NAME).res: $(INDEX_FILE)
	$(GENRB) $(GENRBOPTS) -i $(BUILDDIR) -d $(BUILDDIR) $(INDEX_FILE)


