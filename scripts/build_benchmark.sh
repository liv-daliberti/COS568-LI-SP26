#!/bin/bash
# Build the benchmark executable using the minimal CMakeLists.txt

# Make sure we're in the project root directory
if [ ! -f CMakeLists.txt ]; then
    echo "Error: This script must be run from the project root directory"
    exit 1
fi

BUILD_DIR="${BUILD_DIR:-build}"

echo "Creating and entering build directory: ${BUILD_DIR}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring build..."
cmake -DCMAKE_BUILD_TYPE=Release ..
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed"
    exit 1
fi

echo "Building benchmark..."
BUILD_JOBS="${MILESTONE3_BUILD_JOBS:-${SLURM_CPUS_PER_TASK:-$(nproc)}}"
make -j "$BUILD_JOBS"
BUILD_RESULT=$?

if [ $BUILD_RESULT -ne 0 ]; then
    echo "Error: Build failed"
    exit 1
fi

# Verify that the benchmark binary was created
if [ ! -f benchmark ]; then
    echo "Error: Build completed but benchmark binary was not created"
    exit 1
fi

echo "Build successful! Binary created at: $(pwd)/benchmark"

# Verify the binary doesn't include unwanted index implementations
# Use more specific patterns to avoid false positives
echo "Verifying that the binary contains only the milestone benchmark implementations..."
UNWANTED_REFERENCES=$(nm benchmark | grep -E "benchmark_64_rmi|benchmark_64_art|benchmark_64_alex|benchmark_64_mabtree|benchmark_64_wormhole|benchmark_64_fast|benchmark_64_finedex|benchmark_64_xindex")

if [ -z "$UNWANTED_REFERENCES" ]; then
    echo "Verification successful: No unwanted index implementations found."
else
    echo "Warning: Found references to unwanted index implementations:"
    echo "$UNWANTED_REFERENCES"
    exit 1
fi

echo "Minimal benchmark binary is ready for DynamicPGM, HybridPGMLIPP, B+Tree and LIPP benchmarking!"
