TOPTARGETS := all clean

SUBDIRS := continuous_scan finite_scan finite_scan_with_trigger fft_scan multi_hat_synchronous_scan

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

