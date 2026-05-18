// Phase 3 — CUDA kernel placeholder
// This file will contain the parallelized Variance of Laplacian computation.
// Enable CUDA in CMakeLists.txt before implementing.

// #include <cuda_runtime.h>
// #include <device_launch_parameters.h>
//
// __global__ void varianceOfLaplacianKernel(
//     const uchar* __restrict__ frame,
//     float* __restrict__ sharpnessMap,
//     int width, int height, int kernelRadius)
// {
//     int x = blockIdx.x * blockDim.x + threadIdx.x;
//     int y = blockIdx.y * blockDim.y + threadIdx.y;
//     if (x >= width || y >= height) return;
//
//     // TODO: implement per-pixel Laplacian convolution on GPU
// }
