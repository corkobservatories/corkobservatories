
.. toctree::
   :maxdepth: 3
   
##########################
CORK and BPR Field Guide
##########################

*****************************
Crib sheets
*****************************

.. _sect_downloadData:

Simple health check and download
=================================

#. Make sure laptop clock is synchronized (see :ref:`sect_sync_NTP`)
#. Create a working directory (see :ref:`sect_workingDir`)
#. Connect instrument to computer (see :ref:`sect_InstrumentConnection`)
#. Turn on communication and power **after** connection is established
   make sure power and comms are off before ODI connector is removed from
   parking position
#. Run mlterm, making sure to use a reasonable log-file name (see :ref:`sect_starting_mlterm`)
#. Get instrument stats ``F``
    #. Take note of clock drift (do **not** syncronize the clock, later on)
    #. Take note of predicted download times

#. Get basic logger settings ``I``
    #. Take note of download baud-rate (115200 for NEPTUNE instruments)
    #. Take note of control baud-rate  (115200 for NEPTUNE instruments)
    
#. Consider to increase download speed (see :ref:`sect_increaseDownloadSpeed`)
#. Download data ``D``, making sure to use a reasonable file name
#. Get instrument stats ``F`` again
#. Deploy instrument and quit ``Z`` 
    PI might suggests real-time monitoring while following next steps
#. Backup data (\*.raw) to USB key  (see :ref:`sect_backupData`)
#. Check data integrity
    #. run ``mlbin xxxx.raw`` 
        strips off file system and generate xxxx.bin
        (there should be no errors)
    #. run ``dumpBin.py -a -n -t -s xxxx.bin > xxxx.dmp``
        Does some integrity checking and creates ASCII dump
    #. run ``tail xxxxx.dmp`` to see statistics
        Discuss with PI
    
#. Contact PI and discuss whether to clear memory
#. Backup \*.log files and sent \*.log and \*.raw files to PI
#. Make sure the communication and power lines are turned off **before**
   the under water mateable connector is disconnected.

*********
Howto's
*********

Linux
======

Working with the :term:`HP-Mini` s that are used to download data from :term:`CORK` s and :term:`BPR` s requires some basic Linux skills. This section provides some recipes for common tasks.

.. _fig_LinuxSession:

.. figure:: screenshots/LinuxScreenOverview.*
     :align: center
     
     Linux session with terminal window.

.. _sect_openTerminal:

Open a terminal window
------------------------
Click on the terminal icon

.. image:: screenshots/ShellIcon.*

to open a command-line terminal window. If needed you can create tabs with additional terminals from the ``File`` menu.

.. _sect_basicLinuxCommands:

Basic Linux Commands
-----------------------

``pwd``
    Prints the current working directory
    
``ls``
    Lists the files in the current directory. Use ``ls -lh`` to get more details about the files.
    
``mkdir``
    Creates a new subdirectory (e.g. ``mkdir TestDirectory``)
    
``rm``
    Deletes specified files. ``rm -r`` also deletes directories. **Careful**, there is no way back!!!
    
``cd``
    Change into specified directory. As with all other commands, pressing ``Tab`` does an auto-complete--try it!
    
    
.. _sect_workingDir:

Setup working directory
------------------------
Before you start to communicate with a CORK or BPR, create a working directory under ``/home/corks/Cruises`` 
or ``/home/corks/Tests``--what ever seems more appropriate. If you are on a cruise you might 
consider creating subdirectories for individual instruments. In a terminal window change the 
directory to the working directory and execute ``cp /home/corks/corkobservatory/CalibrationFiles/\*.txt ./`` 
to copy some calibration files into your working directory (see :ref:`sect_CalibrationFiles`). 
Now, you're all set to begin communication with the logger, data download, and post-processing.

If all of this is not clear to you, you might want to read :ref:`sect_openTerminal` 
and :ref:`sect_basicLinuxCommands` first. 


.. _sect_backupData:

Backup data
------------------------
After downloading data from an instrument and before clearing the memory,
make sure that you make at least one backup copy of the data is on a USB key.
To make a backup follow these steps:

#. Insert USB key (it will be mounted under /media)
#. Find out the name/directory of the drive (``ls /media``)
#. Copy files ``cp file1.raw [file2.log ...] /media/nameOfUSB`` (``cp -R`` let's you copy directories recursively)
#. Unmount USB key (click computer icon, hover over device--e.g. KINGSTON, click eject symbol)
   
   .. image:: screenshots/UnmountUSB.*
   
#. Now the eject symbol should have disappeared and you can safely remove the device

.. _sect_sync_NTP:

Sync to NTP
------------

#. Boot up the :term:`HP-Mini` and make sure that wireless networking is enabled (both LEDs on front are blue)
   if you do not have a cabled connection.
#. Make sure you are connected to a network
    #. Hover over icon on the bottom tool bar (globe, or wireless signal) and check if eth0 (cable) or eth1 (wireless are connected)
       
       .. image:: screenshots/NetworkStatus.*
       
    #. If no connection is up, click on the globe and connect to a network (e.g. TGT if you happen to be on the Thompson)
 
#. Open a terminal window and monitor the NTP log file (``tail --follow /var/log/ntp`` -- ``Ctrl-c`` to quit)
    #. Look for ``synchronized to xxx.xxx.xxx.xxx, stratum ?`` that happened within the last 30 min or so.
    #. If you do not see anything, try to restart NTP (``sudo /sbin/service ntp restart`` -- you will need the root password)
    #. If after a few minutes you still do not see any signs of synchronization, you might have to :ref:`sect_setup_NTP`

    
.. _sect_setup_NTP:

Setup NTP
----------
.. todo:: Add section about NTP setup    

Instrument setup
=================

.. _sect_InstrumentConnection:

Connecting the Instrument
--------------------------

Most modern CORKs and BPR have an RS-422 serial interface. We found that :term:`Moxa UPort` USB-to-RS422 converters provide the most reliable connections. 

mlterm
=======

.. _sect_starting_mlterm:

Starting mlterm
----------------

Before running mlterm in a command-line terminal, make sure you are in a directory that is appropriately setup (see :ref:`sect_workingDir`). If you simply run ``mlterm``, the session will be logged (appended) to a logfile named after the current date. It is advisable to use the ``-l myLogfile.log`` command-line option, to choose a descriptive name. On startup, mlterm tries all possible baudrates to connecte to the instrument. If the baudrate is known (115200 for all NEPTUNE instruments) the ``-b 115200`` option provides a quicker initialization. As shown in :ref:`fig_LinuxSession`, the instrument should wake up after a short period of time and after hitting ``Enter`` the high-level mlterm menu will be presented.

.. _sect_increaseDownloadSpeed:

Increase download speed
------------------------
Start mlterm (see :ref:`sect_starting_mlterm`) and from high-level menu proceed as follows:

#. Enter low-level mode by pressing ``P`` and ``Enter``
#. Bring up MT01 menu ``Ctrl-s``
#. Choose option to change download baudrate ``3``
#. Choose appropriate download speed (most likely ``8`` 230400)
#. Quit MT01 menu ``Q``
#. Quit low-level interface ``Esc``

Now, your are all set to start the download procedure.

Note, the download baudrate will automatically be set to it's default if you deploy the instrument by quitting mlterm with ``Z`` or after a timeout period of 30 minutes. 


Data post-processing
=====================

After downloading raw data from an instrument (see :ref:`sect_downloadData`) the data has to post-processed. 

First, mlbin has to be applied (``mlbin filename.raw``) to turn the \*.raw file into a \*.bin file. The \*.raw files 
still contain the filesystem of the flash card and recovery information in case there were errors in the data transmission.
mlbin strips off the filesystem and tries to fix any errors that might have occured. The \*.bin file only contains data records
in binary counts.

mldat9 (``mldat9 filename.bin``) is the standart utility to calibrate \*.bin data and write the results into 
a \*.dat ASCII file. In order to use mldat9 you have to cd into the directory with the \*.bin file and make sure 
there is also a current set of calibration files in the directory (see :ref:`sect_workingDir`). While running mldat9
you will be ask whether the temperatures are measured using a platinum sensor or a thermistor.
Platinum sensors are mostly used in CORKs and BPRs installed past 2009 and have two-digit serial numbers, while 
themistors have one digit serial numbers. The serial numbers are shown in the sites.txt calibratrion file 
(see :ref:`sect_CalibrationFiles`) and should be noted in the respective \*.par file under 
``~/corkobservatory/ParameterFiles``. Furthermore, you are asked whether the Paroscientific gauge measures
pressure and temperature or pressure only. If you do not know the answer you can lookup the gauge serial numbers 
that are mentionend in sites.txt in parosci.txt and check whether U0, Y1, Y2 are zero (pressure only) or non zeros
(pressure and temperature). Then you have to select a data format 
(typically `` 4. Parosci temperature compensated pressure``) and a site description which is a line from the site.txt
calibration file. The site description has the following format:

=======   ================== ===============  ================= ============== ============= =====
number:   description_rtcID  deploymentMonth  temperatureSerial parosciSerial  parosciSerial ...
3:        1027C_89           Jul10            98                120238         120239        ...
=======   ================== ===============  ================= ============== ============= =====
    
mldat9 automatically removes spikes from the paros pressure and temperature channels that get sometimes 
generated by the measurement system and is convinient to use, but it has certain limitations:

#. mldat9 has problems with partial records that can sometimes occur especially on systems that occasionally switch
   between battery and cabled mode.
#. mldat9 cannot cope with instruments that have a mix between gauges that measure temperature and pressure and pressure
   only. These configurations exist especially for CORKs which have many gauges and are designed to operated in cabled
   mode.

To cope with these problems two Python programs were developed that can be used instead of mldat9 
(see :ref:`sect_install_python`). The first script dumpBin.py can be used fix \*.bin files by eliminating partial
records, remove spikes, find problems, and dump the binary counts into an ASCII format that is human readable and can
be calibrated using a calibrateLogfile.py. 

A common workflow to process ``filename.bin`` using these scripts would be:

#. Fix data problems using 
   ``dumpBin.py -n -s -a -t -b filename_fixed.bin filename.bin > filename.dmp``
   
   * filename_fixed.bin will contain all complete data records
   * filename.dmp will contain ASCII representation of binary data and debugging information
   
#. Dump despiked data (only works with clean data) for calibration
   ``dumpBin.py -d filename_fixed.bin > filename_despiked.dmp
#. Calibrate the data
   ``calibrateLogfile.py -c calibrationName filename_despiked.dmp > filename_despiked.dat``
   
   * calibrateLogfile.py also works with data from stdin, so the last two steps can be combined using a pipe:
     ``dumpBin.py -d filename_fixed.bin | calibrateLogfile.py -c calibrationName > filename_despiked.dat``
   * Run ``calibrateLogfile.py -l`` to get a list of available calibrations. New calibrations can be 
     defined in calibrateData.py.

Run ``dumpBin.py --help`` and ``calibrateLogfile.py --help`` to study all available the commandline options.
   
.. _sect_CalibrationFiles:

mlterm calibration files
-------------------------

There are three different calibration files:

parosci.txt
    Calibration coefficients for Paroscientific pressure gauges

platinum.txt
    Calibration coefficients for platinum housing temperature sensors
    
therms.txt
    Calibration coefficient housing temperature thermistors
    
sites.txt
    Data base that associates deployment locations and times, logger IDs,
    and sensor IDs. This information is used for calibration routines.
    
    **Note**, mldat and mlterm only support only up to 30 lines in sites.txt.
    
.. todo:: Changed mldat (wrt2dat.c -- DEFINE MAXSITES) to support sites.txt files with more than 30 lines. Add same functionality to mldat (formatdata.c) requires CORK simulator for testing...

*********    
Glossary
*********

.. glossary::
    
    AWQ bulkhead connector
        CORK pressure cases usually have an 6 pie AWQ bulkhead connector,
        that connects to an 7-pin ODI or SEACON underwater mateable connector.
        BPR pressure cases usually have an 2 piece AWQ bulkhead connector,
        that connects to a 7-pin ODI connector.

        .. image:: figs/ODI_2_PieAWQ.*
           :width: 25%
           :align: center
        
        ODI to two pie AWQ cable
        
    BPR
        Bottom Pressure Recorder
        
    CORK
        Circulation Obviation Retro-fit Kit
        
    HP-Mini
        There are four HP-Minis setup to do CORK and BPR downloads around the world.
        It is also possible to compile the communication software for Mac and there is 
        an image for VirtualBox that makes it possible to run the software on a Linux virtual machine under windows.
        
        .. image:: figs/HP-Mini.*
           :width: 25%
           :align: center
        
   
    Moxa UPort 
        The Moxa UPort USB-2-RS422 to adapter provides superior download speeds over other brands.
        Unfortunately, the driver only supports older versions of Linux. UPDATE: There is a new driver released in 
        summer 2012. Also, Nic Scott mentioned wrapping Windows drivers under Linux
        
        .. image:: figs/Moxa_UPort_RS-422.*
           :width: 25%
           :align: center
           