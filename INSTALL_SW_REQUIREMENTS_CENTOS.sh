#!/bin/sh
yum -y install gcc
yum -y install gcc-c++
yum -y install boost-devel
yum -y install epel-release
yum -y install root
yum -y install root-gui-fitpanel
yum -y install root-spectrum
yum -y install root-spectrum-painter
yum -y install root-minuit2
yum -y install root-physics
yum -y install python 
yum -y install python-devel
yum -y install root-python
yum -y install python-pip
pip install --upgrade pip
pip install bitarray
pip install crcmod
pip install importlib
pip install pyserial
pip install posix_ipc
pip install numpy
pip install argparse


