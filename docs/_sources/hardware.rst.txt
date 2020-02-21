.. _install:

****************************
Installing the DAQ HAT board
****************************

Installing a single board
*************************

1. Power off the Raspberry Pi.
2. Locate the 4 standoffs.  A typical standoff is shown here:

    .. only:: html

        .. image:: images/standoff-web.jpg

    .. only:: latex

        .. image:: images/standoff.jpg
    
3. Attach the 4 standoffs to the Raspberry Pi by inserting the male threaded portion through the 4 corner holes on the Raspberry Pi from the top and securing them with the included nuts from the bottom.
4. Install the 2x20 receptacle with extended leads (MCC provides a Samtec SSQ-120-03-T-D or equivalent) onto the Raspberry Pi GPIO header by pressing the female portion of the receptacle onto the header pins, being careful not to bend the leads of the receptacle.  The 2x20 receptacle looks like:

    .. only:: html

        .. image:: images/connector-web.jpg

    .. only:: latex

        .. image:: images/connector.jpg
    
5. **The HAT must be at address 0.**  Remove any jumpers from the address header locations A0-A2 on the HAT board.
6. Insert the HAT board onto the leads of the 2x20 receptacle so that the leads go into the holes on the bottom of the HAT board and come out through the 2x20 connector on the top of the HAT board.  The 4 mounting holes in the corners of the HAT board must line up with the standoffs.  Slide the HAT board down until it rests on the standoffs.
7. Insert the included screws through the mounting holes on the HAT board into the threaded holes in the standoffs and lightly tighten them.

.. _multiple:

Installing multiple boards
**************************


Follow steps 1-6 in the single board installation procedure for the first HAT board.  

1. **Connect all desired field wiring to the installed board - the screw terminals will not be accessible once additional boards are installed above it.**
2. Install the standoffs of the additional board by inserting the male threaded portions through the 4 corner holes of the installed HAT board and threading them into the standoffs below.
3. Install the next 2x20 receptacle with extended leads onto the leads of the previous 2x20 receptacle by pressing the female portion of the new receptacle onto the previous receptacle leads, being careful not to bend the leads of either receptacle.
4. Install the appropriate address jumpers onto address header locations A0-A2 of the new HAT board. The recommended addressing method is to have the addresses increment from 0 as the boards are installed, i.e. 0, 1, 2, and so forth.  **There must always be a board at address 0.** The jumpers are installed in this manner (install jumpers where "Y" appears):

.. only:: html

    ===========     ======  ======  ======  ========================
    **Address**     **A0**  **A1**  **A2**  **Jumper Setting**
    -----------     ------  ------  ------  ------------------------
    0                                       .. image:: images/a0.png
                                                :scale: 50%
    1               Y                       .. image:: images/a1.png    
                                                :scale: 50%
    2                       Y               .. image:: images/a2.png    
                                                :scale: 50%
    3               Y       Y               .. image:: images/a3.png    
                                                :scale: 50%
    4                               Y       .. image:: images/a4.png    
                                                :scale: 50%
    5               Y               Y       .. image:: images/a5.png    
                                                :scale: 50%
    6                       Y       Y       .. image:: images/a6.png    
                                                :scale: 50%
    7               Y       Y       Y       .. image:: images/a7.png    
                                                :scale: 50%
    ===========     ======  ======  ======  ========================

.. only:: latex

    ===========     ======  ======  ======  ========================
    **Address**     **A0**  **A1**  **A2**  **Jumper Setting**
    -----------     ------  ------  ------  ------------------------
    0                                       .. image:: images/a0.png
                                                :scale: 30%
    1               Y                       .. image:: images/a1.png    
                                                :scale: 30%
    2                       Y               .. image:: images/a2.png    
                                                :scale: 30%
    3               Y       Y               .. image:: images/a3.png    
                                                :scale: 30%
    4                               Y       .. image:: images/a4.png    
                                                :scale: 30%
    5               Y               Y       .. image:: images/a5.png    
                                                :scale: 30%
    6                       Y       Y       .. image:: images/a6.png    
                                                :scale: 30%
    7               Y       Y       Y       .. image:: images/a7.png    
                                                :scale: 30%
    ===========     ======  ======  ======  ========================

    
5. Insert the new HAT board onto the leads of the 2x20 receptacle so that the leads go into the holes on the bottom of the HAT board and come out through the 2x20 connector on the top of the HAT board.  The 4 mounting holes in the corners of the HAT board must line up with the standoffs.  Slide the HAT board down until it rests on the standoffs.
6. Repeat steps 1-5 for each board to be added.
7. Insert the included screws through the mounting holes on the top HAT board into the threaded holes in the standoffs and lightly tighten them.

