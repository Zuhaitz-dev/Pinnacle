#!/usr/bin/env bash

MAX_RUNS=6
BENCHMARK_FILE="examples/fibonacci.asm" # Or a different file. This is what I am using for testing.
                                        # Just change the script.
RESULTS_FILE="results_benchmark.log"

echo "Cleaning old benchmark files..."
rm -f a.out.bin pmv.log "${RESULTS_FILE}"

benchmark()
{
    echo "**Starting Benchmark for ${BENCHMARK_FILE}**"

    make benchmark

    if [ $? -ne 0 ]; then
        echo "ERROR: Benchmark build failed."
        exit 1
    fi
    
    ./bin/assembler "${BENCHMARK_FILE}"

    if [ $? -ne 0 ]; then
        echo "ERROR: Assembly of ${BENCHMARK_FILE} failed."
        exit 1
    fi

    local counter=1
    
    while [ "$counter" -le "${MAX_RUNS}" ]
    do
        echo -e "\n=================================================" >> "${RESULTS_FILE}"
        echo -e "** Run nº ${counter} **" >> "${RESULTS_FILE}"
        echo "=================================================" >> "${RESULTS_FILE}"

        taskset -c 0 ./bin/simulator >> "${RESULTS_FILE}" 2>&1

        echo -e "\n**pmv.log metrics (Run ${counter})**" >> "${RESULTS_FILE}"
        cat pmv.log >> "${RESULTS_FILE}"

        rm -f pmv.log
        
        counter=$(( counter + 1 ))
    done

    echo "Benchmark complete. Results saved to ${RESULTS_FILE}"

    make clean
}

benchmark
