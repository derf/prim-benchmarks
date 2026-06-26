/*++++++++
Written by Yan Zhu, Jan 2018.

This is SCRIMP++.

Details of the SCRIMP++ algorithm can be found at:
Yan Zhu, Chin-Chia M.Yeh, Zachary Zimmerman, Kaveh Kamgar and Eamonn Keogh,
"Solving Time Series Data Mining Problems at Scale with SCRIMP++", submitted to KDD 2018.

Usage: >> scrimpplusplus InputFileName SubsequenceLength stepsize
InputFileName: Name of the time series file
SubsequenceLength: Subsequence length m
stepsize: Step size ratio s/m. For all the experiments in the paper, stepsize is always set as 0.25.

example input:
>> scrimpplusplus ts_1000.txt 50 0.25

example output:
The code will generate two outputs.
SCRIMP_PLUS_PLUS_New_PreSCRIMP_MatrixProfile_and_Index_50_ts_1000.txt          This is the approximate matrix profile and matrix profile index generated after PreSCRIMP.
SCRIMP_PLUS_PLUS_New_MatrixProfile_and_Index_50_ts_1000.txt                    This is the final/exact matrix profile and matrix profile index, generated when the whole algorithm (PreSCRIMP+SCRIMP) is completed.

The first column of the output file is the matrix profile value.
The second column of the output file is the matrix profile index.
*/
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <math.h>
#include <omp.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include <getopt.h>

#if DFATOOL_TIMING
#include "../../include/timer.h"
Timer timer;
#else
#define start(...)
#define stop(...)
#endif

#if WITH_PERF_LIB
extern "C" {
#include "../../../include/perf-lib.h"
};
#elif WITH_PERF_EXT
extern "C" {
#include "../../../include/perf-ext.h"
};
#else
#define perf_start(...)
#define perf_stop(...)
#endif

#if NUMA
#include "../../../include/numa.h"
#else
#define numa_bind_alloc(size, bitmask) malloc(size)
#define numa_free(data, size) free(data)
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#include "mprofile.h"

typedef struct Params {
	char* input_file;
	int window_size;
	int n_warmup;
	int n_reps;
	int exp;
	int n_threads;
#if NUMA
	struct bitmask* bitmask_in;
	struct bitmask* bitmask_out;
	int numa_node_cpu;
#endif
} Params;

struct Params input_params(int argc, char** argv)
{
	struct Params p;
	p.input_file = (char*)"inputs/randomlist33M.txt";
	p.window_size = 256;
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif

	int opt;
	while ((opt = getopt(argc, argv, "f:i:w:e:x:t:A:B:C:")) >= 0) {
		switch (opt) {
		case 'f':
			p.input_file = strdup(optarg);
			break;
		case 'i':
			p.window_size = atol(optarg);
			break;
		case 'w':
			p.n_warmup = atoi(optarg);
			break;
		case 'e':
			p.n_reps = atoi(optarg);
			break;
		case 't':
			p.n_threads = atoi(optarg);
			break;
#if NUMA
		case 'A':
			p.bitmask_in = numa_parse_nodestring(optarg);
			break;
		case 'B':
			p.bitmask_out = numa_parse_nodestring(optarg);
			break;
		case 'C':
			p.numa_node_cpu = atoi(optarg);
			break;
#endif // NUMA
		default:
			fprintf(stderr, "\nUnrecognized option!\n");
			exit(1);
		}
	}

	return p;
}

bool interrupt = false;
int numThreads, exclusionZone;
int windowSize, timeSeriesLength, ProfileLength;
int *profileIndex, *profileIndex_tmp;
DTYPE *AMean, *ASigma, *profile, *profile_tmp;
std::vector<int> idx;
std::vector<DTYPE> A;

void intHandler(int)
{
	std::cout << '\n'
	          << "[>>] Interrupt request by user..." << '\n';
	interrupt = true;
}

void preprocess()
{
	DTYPE* ACumSum = new DTYPE[timeSeriesLength];
	DTYPE* ASqCumSum = new DTYPE[timeSeriesLength];
	DTYPE* ASum = new DTYPE[ProfileLength];
	DTYPE* ASumSq = new DTYPE[ProfileLength];
	DTYPE* ASigmaSq = new DTYPE[ProfileLength];

	AMean = new DTYPE[ProfileLength];
	ASigma = new DTYPE[ProfileLength];

	ACumSum[0] = A[0];
	ASqCumSum[0] = A[0] * A[0];

	for (int i = 1; i < timeSeriesLength; i++) {
		ACumSum[i] = A[i] + ACumSum[i - 1];
		ASqCumSum[i] = A[i] * A[i] + ASqCumSum[i - 1];
	}

	ASum[0] = ACumSum[windowSize - 1];
	ASumSq[0] = ASqCumSum[windowSize - 1];

	for (int i = 0; i < timeSeriesLength - windowSize; i++) {
		ASum[i + 1] = ACumSum[windowSize + i] - ACumSum[i];
		ASumSq[i + 1] = ASqCumSum[windowSize + i] - ASqCumSum[i];
	}

	for (int i = 0; i < ProfileLength; i++) {
		AMean[i] = ASum[i] / windowSize;
		ASigmaSq[i] = ASumSq[i] / windowSize - AMean[i] * AMean[i];
		ASigma[i] = sqrt(ASigmaSq[i]);
	}

	delete[] ACumSum;
	delete[] ASqCumSum;
	delete[] ASum;
	delete[] ASumSq;
	delete[] ASigmaSq;
}

void streamp()
{

#pragma omp parallel
	{
		DTYPE lastz, distance, windowSizeDTYPE;
		DTYPE *distances, *lastzs;
		int diag, my_offset, i, j;
		size_t ri;

		distances = new DTYPE[ARIT_FACT];
		lastzs = new DTYPE[ARIT_FACT];

		windowSizeDTYPE = (DTYPE)windowSize;

		my_offset = omp_get_thread_num() * ProfileLength;

#pragma omp for schedule(dynamic)
		for (ri = 0; ri < idx.size(); ri++) {
			// select a diagonal

			if (!interrupt) {

				diag = idx[ri];

				lastz = 0;

// calculate the dot product of every two time series values that ar diag away
#pragma omp simd
				for (j = diag; j < windowSize + diag; j++) {
					lastz += A[j] * A[j - diag];
				}

				// j is the column index, i is the row index of the current distance value in the distance matrix
				j = diag;
				i = 0;

				// evaluate the distance based on the dot product
				distance = 2 * (windowSizeDTYPE - (lastz - windowSizeDTYPE * AMean[j] * AMean[i]) / (ASigma[j] * ASigma[i]));

				// update matrix profile and matrix profile index if the current distance value is smaller
				if (distance < profile_tmp[my_offset + j]) {
					profile_tmp[my_offset + j] = distance;
					profileIndex_tmp[my_offset + j] = i;
				}

				if (distance < profile_tmp[my_offset + i]) {
					profile_tmp[my_offset + i] = distance;
					profileIndex_tmp[my_offset + i] = j;
				}
				i = 1;
				j = diag + 1;

				/*while(j < (ProfileLength - ARIT_FACT))
				{
				  #pragma omp simd
				  for(int k = 0; k < ARIT_FACT; k++)
				  {
				    lastzs[k] = (A[k + j + windowSize - 1] * A[k + i + windowSize - 1]) - (A[k + j - 1] * A[k + i - 1]);
				  }

				  lastzs[0] += lastz;
				  #pragma unroll (ARIT_FACT - 1)
				  for(int k = 1; k < ARIT_FACT; k++)
				  {
				    lastzs[k] += lastzs[k-1];
				  }
				  lastz = lastzs[ARIT_FACT - 1];

				  #pragma omp simd
				  for(int k = 0; k < ARIT_FACT; k++)
				  {
				    distances[k] =  2 * (windowSizeDTYPE - (lastzs[k] -  AMean[k+j]  * AMean[k+i] * windowSizeDTYPE) / (ASigma[k+j] * ASigma[k+i]));
				  }

				  #pragma omp simd
				  for(int k = 0; k < ARIT_FACT; k++)
				  {
				    if (distances[k] < profile_tmp[k + my_offset + j])
				    {
				      profile_tmp[k + my_offset + j] = distances[k];
				      profileIndex_tmp [k + my_offset+ j] = i + k;
				    }

				   if (distances[k] < profile_tmp[k + my_offset + i])
				    {
				      profile_tmp[k + my_offset + i] = distances[k];
				      profileIndex_tmp[k + my_offset + i] = j + k;
				    }
				  }
				  i+=ARIT_FACT;
				  j+=ARIT_FACT;
				}

				while(j < ProfileLength)
				{
				  lastz   = lastz + (A[j + windowSize - 1] * A[i + windowSize - 1]) - (A[j - 1] * A[i - 1]);
				  distance = 2 * (windowSizeDTYPE - (lastz -  AMean[j]  * AMean[i] * windowSizeDTYPE) / (ASigma[j] * ASigma[i]));

				  if (distance < profile_tmp[my_offset + j])
				  {
				    profile_tmp[my_offset + j] = distance;
				    profileIndex_tmp [my_offset+ j] = i;
				  }

				  if (distance < profile_tmp[my_offset + i])
				  {
				    profile_tmp[my_offset + i] = distance;
				    profileIndex_tmp[my_offset + i] = j;
				  }
				  i++;
				  j++;
				}*/
			}
		}

		delete[] lastzs;
		delete[] distances;

#pragma omp barrier

		// Reduce the (partial) result
		DTYPE min_distance;
		int min_index;

#pragma omp for schedule(static)
		for (int colum = 0; colum < ProfileLength; colum++) {
			min_distance = std::numeric_limits<DTYPE>::infinity();
			min_index = 0;
#pragma unroll(256)
			for (int row = 0; row < numThreads; row++) {
				if (profile_tmp[colum + (row * ProfileLength)] < min_distance) {
					min_distance = profile_tmp[colum + (row * ProfileLength)];
					min_index = profileIndex_tmp[colum + (row * ProfileLength)];
				}
			}
			profile[colum] = min_distance;
			profileIndex[colum] = min_index;
		}
#pragma omp barrier
	}

	delete (AMean);
	delete (ASigma);
	delete (profile_tmp);
	delete (profileIndex_tmp);
}

int main(int argc, char* argv[])
{
	bool sequentialDiags = false;
	// Creation of time meassure structures
	std::chrono::high_resolution_clock::time_point tprogstart, tstart, tend;
	std::chrono::duration<double> time_load, time_preprocess, time_alloc, time_kernel;

	// Creation of interrupt handler
	struct sigaction act;
	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, NULL);

	struct Params p = input_params(argc, argv);

	omp_set_num_threads(p.n_threads);
	windowSize = p.window_size;

	// Set the exclusion zone
	exclusionZone = (int)(windowSize * 0.25);

	// Set the thread number
	// numThreads = atoi(argv[3]);
	// omp_set_num_threads(numThreads);

	numThreads = omp_get_max_threads();

	// Set computational order
	if (argc > 4)
		sequentialDiags = (strcmp(argv[4], "-s") == 0);

	// Display info through console
	/*
	std::cout << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "///////////////////////// STREAMP //////////////////////////" << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << std::endl;
	*/
	// std::cout << "[>>] Reading File..." << std::endl;

#if NUMA
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
	if (p.bitmask_in != NULL) {
		numa_set_membind(p.bitmask_in);
	}
#endif

	/* Read time series file */
	tstart = std::chrono::high_resolution_clock::now();
	//  tprogstart = tstart;

	std::stringstream outfilename_num;
	outfilename_num << windowSize;
	std::string outfilenamenum = outfilename_num.str();
	std::string inputfilename = p.input_file;
	std::string outfilename = "SCRIMP_PLUS_PLUS_New_MatrixProfile_and_Index_" + outfilenamenum + "_" + inputfilename;

	loadTimeSeriesFromFile(inputfilename, A, timeSeriesLength);

	tend = std::chrono::high_resolution_clock::now();
	time_load = tend - tstart;
	// std::cout << "[OK] Read File Time: " << std::setprecision(std::numeric_limits<double>::digits10 + 2) << time_load.count() << " seconds." << std::endl;

#if NUMA
	numa_node_in = numa_get_node_of_page(static_cast<void*>(A.data()), "A");
#endif

	// Set Matrix Profile Length
	ProfileLength = timeSeriesLength - windowSize + 1;

	// Display info through console
	/*
	std::cout << std::endl;
	std::cout << "------------------------------------------------------------" << std::endl;
	std::cout << "************************** INFO ****************************" << std::endl;
	std::cout << std::endl;
	std::cout << " Time series length: " << timeSeriesLength << std::endl;
	std::cout << " Window size:        " << windowSize << std::endl;
	std::cout << " Exclusion zone:     " << exclusionZone << std::endl;
	std::cout << " Profile length:     " << timeSeriesLength << std::endl;
	std::cout << " Max avail. threads: " << numThreads << std::endl;
	std::cout << " Sequential order:   ";
	if (sequentialDiags)
	        std::cout << "true" << std::endl;
	else
	        std::cout << "false" << std::endl;
	std::cout << std::endl;
	std::cout << "------------------------------------------------------------" << std::endl;
	std::cout << std::endl;
	*/

#if NUMA
	if (p.bitmask_out != NULL) {
		numa_set_membind(p.bitmask_out);
	}
#endif

	for (int i = 0; (i < p.n_warmup + p.n_reps) && !interrupt; i++) {

		tstart = std::chrono::high_resolution_clock::now();
		tprogstart = tstart;
		perf_start();

		preprocess();

		perf_stop();
		tend = std::chrono::high_resolution_clock::now();
		time_preprocess = tend - tstart;

#if NUMA
		numa_node_out = numa_get_node_of_page(AMean, "AMean");
#endif

#if WITH_PERF_LIB
		if (i >= p.n_warmup) {
			printf("[::] TS-CPU-preprocess | n_threads=%d e_type=%s n_elements=%d",
			    numThreads, XSTR(DTYPE), timeSeriesLength);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
		}
#endif

		tstart = std::chrono::high_resolution_clock::now();
		perf_start();
		profile = new DTYPE[ProfileLength];
		profileIndex = new int[ProfileLength];

		profile_tmp = new DTYPE[ProfileLength * numThreads];
		profileIndex_tmp = new int[ProfileLength * numThreads];

		for (int i = 0; i < ProfileLength * numThreads; i++)
			profile_tmp[i] = std::numeric_limits<DTYPE>::infinity();

		perf_stop();
		tend = std::chrono::high_resolution_clock::now();
		time_alloc = tend - tstart;

#if WITH_PERF_LIB
		if (i >= p.n_warmup) {
			printf("[::] TS-CPU-alloc | n_threads=%d e_type=%s n_elements=%d",
			    numThreads, XSTR(DTYPE), timeSeriesLength);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
		}
#endif

		// Random shuffle the diagonals
		idx.clear();
		for (int i = exclusionZone + 1; i < ProfileLength; i++)
			idx.push_back(i);

		if (!sequentialDiags)
			std::random_shuffle(idx.begin(), idx.end());

		/******************** SCRIMP ********************/
		// std::cout << "[>>] Performing STREAMP..." << std::endl;
		tstart = std::chrono::high_resolution_clock::now();
		perf_start();

		streamp();

		perf_stop();
		tend = std::chrono::high_resolution_clock::now();
		time_kernel = tend - tstart;

#if WITH_PERF_LIB
		if (i >= p.n_warmup) {
			printf("[::] TS-CPU-streamp | n_threads=%d e_type=%s n_elements=%d",
			    numThreads, XSTR(DTYPE), timeSeriesLength);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
		}
#endif

#if DFATOOL_TIMING
		if (i >= p.n_warmup) {
			std::chrono::duration<double> time_elapsed = tend - tprogstart;
			printf("[::] TS-CPU | n_threads=%d e_type=%s n_elements=%d",
			    numThreads, XSTR(DTYPE), timeSeriesLength);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" | throughput_preproc_MBps=%f throughput_preproc_MOpps=%f latency_preproc_s=%f",
			    timeSeriesLength * sizeof(DTYPE) / (time_preprocess.count() * 1e6), timeSeriesLength / (time_preprocess.count() * 1e6), time_preprocess.count());
			printf(" throughput_alloc_MBps=%f throughput_alloc_MOpps=%f latency_alloc_s=%f",
			    timeSeriesLength * sizeof(DTYPE) / (time_alloc.count() * 1e6), timeSeriesLength / (time_alloc.count() * 1e6), time_alloc.count());
			printf(" throughput_streamp_MBps=%f throughput_streamp_MOpps=%f latency_streamp_s=%f", timeSeriesLength * sizeof(DTYPE) / (time_kernel.count() * 1e6), timeSeriesLength / (time_kernel.count() * 1e6), time_kernel.count());
			printf(" throughput_MBps=%f throughput_MOpps=%f latency_s=%f\n", timeSeriesLength * sizeof(DTYPE) / (time_elapsed.count() * 1e6), timeSeriesLength / (time_elapsed.count() * 1e6), time_elapsed.count());
		}
#endif

		delete profile;
		delete profileIndex;
	}
}
