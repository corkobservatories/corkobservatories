#!/bin/bash

CURR_DIR = `pwd`

cd ~/corkobservatories/C-tools/mlterm
make
sudo make install

cd ~/corkobservatories/C-tools/mlbin
make
sudo make install

cd ~/corkobservatories/C-tools/mldat
make
sudo make install

cd $CURR_DIR

