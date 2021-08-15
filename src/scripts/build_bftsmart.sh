#!/bin/bash
cd ../store/bftsmartstore/library
rm -rf jars
mkdir bin
ant
mkdir jars
cp bin/BFT-SMaRt.jar jars
cp lib/* jars
