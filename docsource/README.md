# MCC DAQ HAT Documentation
This is the documentation source for the DAQ HAT library.
The output of this documentation is stored in the \docs directory and at:

https://mccdaq.github.io/daqhats.

Generating Documentation (not normally needed)
==============================================
Follow these steps to generate the documentation for the MCC HAT library:

1. Install breathe, pip, doxygen, sphinx, and sphinx_rtd_theme.

    ```
    sudo apt install doxygen python-pip
    sudo pip install breathe sphinx sphinx_rtd_theme
    ```
2. Create the C documentation with doxygen (inside the docsource directory)

    ```
    cd ~/daqhats/docsource
    make c
    ```
3. Run sphinx to generate the documentation.

    ```
    make html
    ```
4. The resulting documentation can be found in the docsource/_build/html directory.  It can be moved to the docs directory with:

    ```
    make install
    ```
    
Some additional large packages are required (approximately 1.5 GB) to build the pdf output. 

1. Install texlive, latexmk, and texlive-latex-extra.

    ```
    sudo apt install texlive latexmk texlive-latex-extra
    ```
2. Repeat steps #2 and 3 above to create the C and html documentation.
3. Run sphinx to generate the pdf output.

    ```
    make latexpdf
    ```
4. The pdf output is stored in docsource/_build/latex directory. It will be copied to the \docs directory along with the html documentation using:

    ```
    make install
    ```
