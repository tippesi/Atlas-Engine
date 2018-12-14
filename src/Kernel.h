#ifndef KERNEL_H
#define KERNEL_H

#include "System.h"
#include <vector>

class Kernel {

public:
    Kernel();

    void CalculateGaussian(float sigma, uint32_t size);

    void CalculateBox(uint32_t size);

    void Get(vector<vector<float>>*& weights, vector<vector<ivec2>>*& offsets);

    void GetLinearized(vector<float>*& weights, vector<float>*& offsets);

private:
	float Gaussian(float x, float y, float mean, float sigma);

    vector<vector<float>> weights;
    vector<vector<ivec2>> offsets;

    vector<float> weightsLinearized;
    vector<float> offsetsLinearized;

	bool changed;

};

#endif