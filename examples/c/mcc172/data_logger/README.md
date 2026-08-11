# Data Logger Example

## About
The **Data Logger** example shows how to acquire data from the **MCC 172 HAT**, display the data on
a strip chart, calculate and display FFT data, and log the data to a CSV file.

## Dependencies
- **GTK+** cross-platform toolkit for creating graphical user interfaces.
- **GtkDatabox** widget used to display two-dimensional data.
- **D-Bus AT-SPI** protocol.
- **Kiss FFT** FFT library
- Monitor connected to the **Raspberry Pi** to configure acquisition options and view acquired data

## Install the Dependencies
Install required packages: 
  ```sh
  sudo apt install libgtk-3-dev at-spi2-core autoconf libtool
  ```
Install **Kiss FFT**:
  ```sh
  cd ~
  git clone  https://github.com/mborgerding/kissfft.git
  cd kissfft
  make; sudo make install
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
  cd ~/daqhats/examples/c/mcc172/data_logger/logger
  make
  ./logger
  ```

## Support/Feedback
Contact technical support through our [support page](https://www.mccdaq.com/support/support_form.aspx).

## More Information
- GTK+: https://www.gtk.org/
- GTKDataBox: https://sourceforge.net/projects/gtkdatabox/
- Kiss FFT: https://sourceforge.net/projects/kissfft/
