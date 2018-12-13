#ifndef KERNEL_H
#define KERNEL_H

#include "System.h"

class Kernel {

public:
    Kernel();

    void CalculateGaussian(float sigma, int32_t size);

    void CalculateBox(int32_t size);

    void Get(int32_t* size, float* weights, float* offsets);

    void GetLinearized(int32_t* size, float* weights, float* offsets);

private:
    float* weights;
    float* offsets;

    float* weightsLinearized;
    float* offsetsLinearized;

};

#endif