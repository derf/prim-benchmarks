all: va

va: vec_add.cu
	/usr/local/cuda/bin/nvcc vec_add.cu -I/usr/local/cuda/include -lm -O2 -o va

clean:
	rm -f va

.PHONY: all clean
