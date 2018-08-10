# DataLogger Example

## About
The **DataLogger** example shows how to acquire data from the **MCC 118 HAT**, display the data on 
a strip chart, and log the data to a CSV file. 
This example can be run from a terminal window, or accessed with an IDE such as **Geany** or **CodeBlocks**. 

## Dependencies
- **GTK+** cross-platform toolkit for creating graphical user interfaces.
- **GtkDatabox** widget used to display two-dimensional data.
- **D-Bus AT-SPI** protocol.
- Monitor connected to the **Raspberry Pi** to configure acquisition options and view acquired data 
 
 > Project files are supplied for **Geany** and **CodeBlocks**. 

## Install the Dependencies
Install **GTK+**: 
  ```sh
    sudo apt-get install libgtk-3-dev
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
 Install **D-Bus AT-SPI**:
  ```sh
    sudo apt-get install at-spi2-core
  ```

> You may need to run ldconfig after installing the dependencies.
  ```sh
    sudo ldconfig
  ```
  
## Running the example from a terminal
To run the example from a terminal window, enter the following commands:
  ```sh
    cd ~/daqhats/examples/c/mcc118/data_logger/logger
    make
    ./logger
  ```
## Running the example from Geany
**Geany** is one of the editors supplied with Raspbian, so is probably already installed on your
**Raspberry Pi**. If not, or to check for updates, run the following in a terminal:
  ```sh
    sudo apt-get install geany
  ```
To build the project, press Shift+F9 or select Make from the Build menu. Select the `logger.c` 
file, then press F5 or select Execute from the Build menu.

## Running the example from CodeBlocks
To install **CodeBlocks**, run the following in a terminal:
  ```sh
    sudo apt-get install codeblocks
  ```
Click `Build`, then `Run` to run the example.

## Support/Feedback
Contact technical support through our [support page](https://www.mccdaq.com/support/support_form.aspx). 

## More Information
- GTK+: https://www.gtk.org/ 
- GTKDataBox: https://sourceforge.net/projects/gtkdatabox/
- CodeBlocks: http://www.codeblocks.org/
- Geany: https://www.geany.org/