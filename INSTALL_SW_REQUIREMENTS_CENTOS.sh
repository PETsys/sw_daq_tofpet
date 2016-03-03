#!/bin/sh
yum -y install gcc \
gcc-c++ \
boost-devel \
epel-release \
root \
root-gui-fitpanel \
root-spectrum \
root-spectrum-painter \
root-minuit2 \
root-physics \
python \
python-devel \
root-python \
python-pip \
kernel-devel \

pip install --upgrade pip
pip install bitarray
pip install crcmod
pip install importlib
pip install pyserial
pip install posix_ipc
pip install numpy
pip install argparse


