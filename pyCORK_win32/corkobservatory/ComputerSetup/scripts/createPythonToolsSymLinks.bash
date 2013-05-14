#!/bin/bash

# Make sure users bin directory exists
if [ ! -d FILE ]
   then
      mkdir ~/bin
fi

# Create links assuming that users checked out code in ~/corkobservatory
ln -s ~/corkobservatory/Python-tools/calibrate/dumpBin.py ~/bin/dumpBin.py
ln -s ~/corkobservatory/Python-tools/calibrate/calibrateLogfile.py ~/bin/calibrateLogfile.py
