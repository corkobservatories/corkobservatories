
.. toctree::
   :maxdepth: 3
   
##########################
CORK Computer Setup
##########################

*****************************
Required software
*****************************

A computer running Linux is the simplest platform to install and run the CORK software on. 
It is also known to run on Mac computers and most data data processing can be done on Windows machines,
but mlterm the programm used to communicate with CORKs and download the data does not work under windows 
and the necessary development environment to compile various C-programs might be challenging to setup on Mac.

.. _sect_checkout_corkobservatory:

Check out the CORK software repository
=======================================


CORK software, calibration and parameter files are maintainded at the 
`CORK Observatory Software Repository. <https://sourceforge.net/projects/corkobservatory/>`_

It is best to have svn installed to check out the code. Check if svn is installed by running ``svn --help``.
If there is not help displayed you have to install svn e.g. by running ``sodu yum install subversion``.

Now, go into the home directory of the cork user (``cd ~``) and run

``svn co https://corkobservatory.svn.sourceforge.net/svnroot/corkobservatory corkobservatory``

This will create a ``corkobservatory`` directory that contains program source codes, calibration information, etc.
To update these files to the latest version you can cd into the ``corkobservatory`` directory and run ``svn update`` at any time 
when you have an internet connection.

Compile and install mlterm, mlbin, mldat9
==========================================

``mlterm``, ``mlbin`` and ``mldat9`` are the core utilities (programmed in C) to setup CORKs and download the data, strip unneccessary 
filesystem information from the downloaded *.raw files, and to calibrate the data contained in the *.bin files, respectively.
In some special cases, when there are problems with data consistency or with special instrument setups you need to use
some Python scripts, that will be discussed later, instead of mldat9.

Since the core utilities are supplied as C source code, you need to have a C-compiler and basic development tools installed.
cd into the ``mlterm``, ``mlbin``, and ``mldat`` directories under ``~/corkobservatory/C-tools`` and run

#. make
#. sudo make install

in each directory to compile and install the programs individually.

NOTE: there is also a variant of mlterm ``mlterm_vpn`` that gets automatically installed with mlterm that is for use with
instruments connected over connections with high latency (e.g. TCP/IP, VPN,...).

Install RS-422 adapter driver if necessary
===========================================

Many USB/RS-422 adapters run under Linux out of the box, but the :term:`Moxa UPort`-1130, which we found is the
most reliable adapter, requires the compilation/installation of a seperate driver. We had problems in the past, because the 
driver was not compatible with current Linux distributions, but fortunately, Moxa did update the driver in summer 2012.

So if you want to use a Moxa adapter make sure to download and install the latest diver either from their 
`website <http://www.moxa.com/support/download.aspx?type=support&id=776>`_ or use the copy that is stored in the software repository
(``~/corkobservatory/ComputerSetup/drivers/MoxaUPort``). Follow the instructions in the ``readme.txt`` file under "Module driver configuration".

If the driver install succeeded running ``mlterm`` with the Moxa UPort plugged into a USB port should start an mlterm
session with mlterm trying to connect to the instrument. If the adapter is not recognized by the OS mlterm will fail
with the following message:
 
``/dev/ttyUSB0: No such file or directory``

.. _sect_install_python:

Install dependencies for Python scripts
========================================


The Python scripts need

#. Python (tested with 2.6 and 2.7)
#. numpy
#. matplotlib (optionally for plotting)

So first check if Python is already installed ``python --version`` and install it with e.g. 
``sudo yum install python`` if not. Now you can try to install the numpy and matplotlib packages 
of your distribution e.g.:

* ``sudo yum install python-numpy``
* ``sudo yum install python-matplotlib``

or install easy_install e.g. ``sudo yum install python-setuptools`` and then run:

* ``sudo easy_install numpy``
* ``sudo easy_install matplotlib``

Now, cd into the ``~/corkobservatory/ComputerSetup/scripts``  directory 
(see  :ref:`sect_checkout_corkobservatory` for how to set it up) and run ``./createPythonToolsSymLinks.bash`` to add 
symbolic links to the python scripts ``dumpBin.py`` and ``calibrateLogfile.py`` to your path. If everything worked out,
running ``dumpBin.py --help`` and ``calibrateLogfile.py --help`` will display the respective help information.
