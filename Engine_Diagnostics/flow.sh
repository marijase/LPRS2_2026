#!/bin/bash

git clone https://github.com/Comer352L/FreeSSM
pushd FreeSSM
qmake
make debug
make translation