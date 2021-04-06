# Tools and Firmware

This directory contains library tools, test applications, and firmware files.

## Firmware Version History

### MCC 118
1.03:
   - Fixes occasional data corruption issue introduced in 1.02.

1.02:
   - Fixes problem with intermittent time shift in a sample on one channel when
     sampling multiple channels.

1.01:
   - Initial release.

### MCC 128
1.01:
   - Fixes issue where continuous scans could have a hardware or buffer overrun after 
     several minutes.

   - Fixes issue with a hang when stopping a scan on devices configured for external clock.

1.00:
   - Initial release.

### MCC 172
1.01:
   - Fixes issue with the first scan after changing the trigger mode.

1.00:
   - Initial release.
