TYPE?=UINT64

all:
	/usr/local/cuda-13.1/bin/nvcc app_baseline.cu -I/usr/local/cuda-13.1/include -lm -o red -D${TYPE}

clean:
	rm red
