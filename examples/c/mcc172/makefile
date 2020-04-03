TOPTARGETS := all clean

SUBDIRS := continuous_scan finite_scan fft_scan data_logger

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

