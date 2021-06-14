#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

// spread of each voice (with max detune) in semitones from central note
// could maybe automate this generation instead?
static vector<vector<double>> allDetuneSemitones = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, -1.0 / 3.0, 1.0 / 3.0, 1.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, -0.5, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0},
    {-1.0, -0.6, -0.2, 0.2, 0.6, 1.0, 0.0, 0.0},
    {-1.0, -2.0 / 3.0, -1.0 / 3.0, 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0, 0.0},
    {-1.0, -1.0 + 2.0 / 7.0, -1.0 + 4.0 / 7.0, -1.0 + 6.0 / 7.0, 1.0 - 6.0 / 7.0, 1.0 - 4.0 / 7.0, 1.0 - 2.0 / 7.0, 1.0}
};