include ../Make.config


SUBDIRS = c128 z88 x1 msx svi nc100 rex zx gb s1mp3 cpc sos ace pps tvc trs80 ticalc mtx oz enterprise cpm osca zx81 zxn zx-common ts2068 newbrain
# Directories that need a target passed down
CLEANDIRS = $(SUBDIRS:%=clean-%) $(TARGETDIRS:%=clean-%)

all: $(SUBDIRS)


subdirs-all: $(SUBDIRS) $(TARGETDIRS)

subdirs-clean: $(SUBDIRS_CLEAN)


clean: subdirs-clean
	$(RM) */*.o */*/*.o  */*/*/*.o

subdirs-clean: $(CLEANDIRS)


.PHONY:	subdirs-all $(SUBDIRS) $(SUBDIRS_CLEAN)

$(SUBDIRS):
	$(MAKE) -C $@ all

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
