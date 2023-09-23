export V ?= 0
include conf.mk

.PHONY:ta
ta:
ifeq ($(SECURITY),1)
	$(MAKE) -C ta1 CROSS_COMPILE=$(CROSS_COMPILE) O=$(TA_EXPORT) TA_DEV_KIT_DIR=$(TA_DEV_KIT_DIR) LDFLAGS=""
	$(MAKE) -C ta2 CROSS_COMPILE=$(CROSS_COMPILE) O=$(TA_EXPORT) TA_DEV_KIT_DIR=$(TA_DEV_KIT_DIR) LDFLAGS=""
	cp ta1/out/eff27aea-8019-8f98-4963-406233b51e44.ta /lib/optee_armtz/
	cp ta2/out/ebf27aea-8019-8f98-4963-406233b51e44.ta /lib/optee_armtz/
endif

.PHONY: darknet
darknet:
	$(MAKE) -C host 

.PHONY: all
all: darknet ta
	

.PHONY: clean
clean:
	$(MAKE) -C host clean
ifeq ($(SECURITY),1)
		rm -rf ta1/out
		rm -rf ta2/out
endif

.DEFAULT_GOAL := all 