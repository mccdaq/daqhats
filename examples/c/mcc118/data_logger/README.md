# DataLogger Example

## About
The **DataLogger** example shows how to acquire data from the **MCC 118 HAT**, display the data on 
a strip chart, and log the data to a CSV file. 

## Dependencies
- **GTK+** cross-platform toolkit for creating graphical user interfaces.
- **GtkDatabox** widget used to display two-dimensional data.
- **D-Bus AT-SPI** protocol.
- Monitor connected to the **Raspberry Pi** to configure acquisition options and view acquired data

## Install the Dependencies
Install required packages: 
  ```sh
  sudo apt install libgtk-3-dev at-spi2-core autoconf libtool
  ```
Install **GTKDatabox**:
  ```sh
  cd ~
  git clone https://github.com/erikd/gtkdatabox.git
  cd gtkdatabox
  ./autogen.sh
  ./configure
  sudo make install
  ```
Run ldconfig after installing the dependencies.
  ```sh
  sudo ldconfig
  ```

## Running the example
To run the example, enter the following commands:
  ```sh
    cd ~/daqhats/examples/c/mcc118/data_logger/logger
    make
    ./logger
  ```

This example uses the Kiss FFT library (already included, but see
https://github.com/mborgerding/kissfft for more information), which has the
following license information:
```
Copyright (c) 2003-2010 Mark Borgerding . All rights reserved.

KISS FFT is provided under:

  SPDX-License-Identifier: BSD-3-Clause

Being under the terms of the BSD 3-clause "New" or "Revised" License,
according with:

  LICENSES/BSD-3-Clause
```

## Support/Feedback
Contact technical support through our [support page](https://www.mccdaq.com/support/support_form.aspx).

## More Information
- GTK+: https://www.gtk.org/
- GTKDataBox: https://sourceforge.net/projects/gtkdatabox/
