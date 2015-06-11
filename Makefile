default: DSHM.so
	$(MAKE) -f module.mk -C daqd $(MAKEFLAGS)
	$(MAKE) -f module.mk -C aDAQ $(MAKEFLAGS)

DSHM.so: daqd/DSHM.so
	cp -f daqd/DSHM.so DSHM.so

clean: 
	$(MAKE) -f module.mk -C daqd $(MAKEFLAGS) clean
	$(MAKE) -f module.mk -C aDAQ $(MAKEFLAGS) clean

.PHONY: default clean
