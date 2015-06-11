default: 
	$(MAKE) -f module.mk -C daqd $(MAKEFLAGS)
	$(MAKE) -f module.mk -C aDAQ $(MAKEFLAGS)
	cp -f daqd/DSHM.so DSHM.so

clean: 
	$(MAKE) -f module.mk -C daqd $(MAKEFLAGS) clean
	$(MAKE) -f module.mk -C aDAQ $(MAKEFLAGS) clean

.PHONY: default clean
