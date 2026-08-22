# FFT Scan Example

## About
This example demonstrates how to perform finite acquisitions from both channels,
perform FFTs on the data, and find the peaks and harmonics.

## Dependencies
- **Kiss FFT** FFT library

## Install the Dependencies
Install **Kiss FFT**:
  ```sh
  cd ~
  git clone --branch 131.2.0 --depth 1 https://github.com/mborgerding/kissfft.git
  cd kissfft
  make; sudo make install
  ```
Run ldconfig after installing the dependencies.
  ```sh
  sudo ldconfig
  ```

## Running the example
To run the example, open a terminal window and enter the following commands:
  ```sh
  cd ~/daqhats/examples/c/mcc172/fft_scan
  make
  ./fft_scan
  ```

## Support/Feedback
Contact technical support through our
[support page](https://www.mccdaq.com/support/support_form.aspx).

## More Information
- Kiss FFT: https://sourceforge.net/projects/kissfft/

