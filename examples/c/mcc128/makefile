TOPTARGETS := all clean

SUBDIRS := continuous_scan finite_scan finite_scan_with_trigger multi_hat_synchronous_scan single_value_read

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

