# 
#  Copyright (c) 2016 University of Cordoba and University of Illinois
#  All rights reserved.
# 
#  Developed by:    IMPACT Research Group
#                   University of Cordoba and University of Illinois
#                   http://impact.crhc.illinois.edu/
# 
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  with the Software without restriction, including without limitation the 
#  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
# 
#       > Redistributions of source code must retain the above copyright notice,
#         this list of conditions and the following disclaimers.
#       > Redistributions in binary form must reproduce the above copyright
#         notice, this list of conditions and the following disclaimers in the
#         documentation and/or other materials provided with the distribution.
#       > Neither the names of IMPACT Research Group, University of Cordoba, 
#         University of Illinois nor the names of its contributors may be used 
#         to endorse or promote products derived from this Software without 
#         specific prior written permission.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
#  CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
#  THE SOFTWARE.
# 

CXX=g++
CXX_FLAGS=-std=c++11

LIB=-L/usr/lib/ -lm -pthread

DEP=kernel.cpp kernel.h main.cpp support/common.h support/setup.h support/timer.h 
SRC=main.cpp kernel.cpp
EXE=trns

all: trns

trns: ${SRC}
	$(CXX) -O2 $(CXX_FLAGS) $(SRC) $(LIB) -o $(EXE)

trns_O0: ${SRC}
	$(CXX) $(CXX_FLAGS) $(SRC) $(LIB) -o $(EXE)_O0

trns_O2: ${SRC}
	$(CXX) -O2 $(CXX_FLAGS) $(SRC) $(LIB) -o $(EXE)_O2

run: trns
	./trns -w 0 -r 1 -m 16 -n 8 -o 4096 -p 2556

# upstream uses -r 1

run_O0: trns_O0
	./trns_O0 -w 0 -r 50 -m 16 -n 8 -o 4096 -p 2556

run_O2: trns_O2
	./trns_O2 -w 0 -r 50 -m 16 -n 8 -o 4096 -p 2556

clean:
	rm -f $(EXE) ${EXE}_O0 ${EXE}_O2

.PHONY: all run run_O0 run_O2 clean
