#!/bin/sh

sed -i 's!rootdir = "/"!rootdir = "'"$(pwd)"'"!' *.py
