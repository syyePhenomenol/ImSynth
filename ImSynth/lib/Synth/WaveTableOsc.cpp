//
//  Test wavetable oscillator
//
//  Created by Nigel Redmon on 4/31/12
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the wavetable oscillator and code,
//  read the series of articles by the author, starting here:
//  www.earlevel.com/main/2012/05/03/a-wavetable-oscillator—introduction/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//

#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

#include "WaveTableOsc.h"

void makeAllTables(vector<vector<waveTable>>* allTables, float baseFreq) {
    // run loop over every wavetable shape
    for (int n = 0; n < numberOfShapes; n++) {
        // calc number of harmonics where the highest harmonic baseFreq and lowest alias an octave higher would meet
        int maxHarms = sampleRate / (3.0 * baseFreq) + 0.5;

        // round up to nearest power of two
        unsigned int v = maxHarms;
        v--;            // so we don't go up if already a power of 2
        v |= v >> 1;    // roll the highest bit into all lower bits...
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;            // and increment to power of 2
        int tableLen = v * 2 * overSamp;  // double for the sample rate, then oversampling

        vector<myFloat> ar(tableLen), ai(tableLen); // for ifft
        double topFreq = baseFreq * 2.0 / sampleRate;

        for (; maxHarms >= 1; maxHarms >>= 1) {
            if (n == 0) {
                defineSine(tableLen, ar, ai);
            }
            else if (n == 1) {
                defineTriangle(tableLen, maxHarms, ar, ai);
            }
            else if (n == 2) {
                defineSawtooth(tableLen, maxHarms, ar, ai);
            }
            else if (n == 3) {
                defineSquare(tableLen, maxHarms, ar, ai);
            }
            waveTable table = makeWaveTable(tableLen, ar, ai, topFreq);
            (*allTables)[n].push_back(table);
            topFreq *= 2;
            //if (tableLen > constantRatioLimit) // variable table size (constant oversampling but with minimum table size)
            //    tableLen >>= 1;
        }
    }
}

//
// if scale is 0, auto-scales
// returns scaling factor (0.0 if failure), and wavetable in ai array
//
waveTable makeWaveTable(int len, vector<myFloat>& ar, vector<myFloat>& ai, double topFreq)
{
    waveTable table;

    fft(len, ar, ai);

    myFloat max = 0.0;
    float scale = 0.0;

    for (int idx = 0; idx < len; idx++) {
        myFloat temp = fabs(ai[idx]);
        if (max < temp) {
            max = temp;
        }
    }

    scale = 1.0 / max * .999;

    // normalize and cast to a float vector
    vector<float> wave(len);
    for (int idx = 0; idx < len; idx++)
        wave[idx] = ai[idx] * scale;

    wave.push_back(wave.front());

    table.topFreq = topFreq;
    table.waveTableLen = len;
    table.waveTable_ = wave;

    return table;
}

//
// fft
//
// I grabbed (and slightly modified) this Rabiner & Gold translation...
//
// (could modify for real data, could use a template version, blah blah--just keeping it short)
//
void fft(int N, vector<myFloat>& ar, vector<myFloat>& ai)
/*
 in-place complex fft
 
 After Cooley, Lewis, and Welch; from Rabiner & Gold (1975)
 
 program adapted from FORTRAN 
 by K. Steiglitz  (ken@princeton.edu)
 Computer Science Dept. 
 Princeton University 08544          */
{    
    int i, j, k, L;            /* indexes */
    int M, TEMP, LE, LE1, ip;  /* M = log N */
    int NV2, NM1;
    myFloat t;               /* temp */
    myFloat Ur, Ui, Wr, Wi, Tr, Ti;
    myFloat Ur_old;
    
    // if ((N > 1) && !(N & (N - 1)))   // make sure we have a power of 2
    
    NV2 = N >> 1;
    NM1 = N - 1;
    TEMP = N; /* get M = log N */
    M = 0;
    while (TEMP >>= 1) ++M;
    
    /* shuffle */
    j = 1;
    for (i = 1; i <= NM1; i++) {
        if(i<j) {             /* swap a[i] and a[j] */
            t = ar[j-1];     
            ar[j-1] = ar[i-1];
            ar[i-1] = t;
            t = ai[j-1];
            ai[j-1] = ai[i-1];
            ai[i-1] = t;
        }
        
        k = NV2;             /* bit-reversed counter */
        while(k < j) {
            j -= k;
            k /= 2;
        }
        
        j += k;
    }
    
    LE = 1.;
    for (L = 1; L <= M; L++) {            // stage L
        LE1 = LE;                         // (LE1 = LE/2) 
        LE *= 2;                          // (LE = 2^L)
        Ur = 1.0;
        Ui = 0.; 
        Wr = cos(M_PI/(float)LE1);
        Wi = -sin(M_PI/(float)LE1); // Cooley, Lewis, and Welch have "+" here
        for (j = 1; j <= LE1; j++) {
            for (i = j; i <= N; i += LE) { // butterfly
                ip = i+LE1;
                Tr = ar[ip-1] * Ur - ai[ip-1] * Ui;
                Ti = ar[ip-1] * Ui + ai[ip-1] * Ur;
                ar[ip-1] = ar[i-1] - Tr;
                ai[ip-1] = ai[i-1] - Ti;
                ar[i-1]  = ar[i-1] + Tr;
                ai[i-1]  = ai[i-1] + Ti;
            }
            Ur_old = Ur;
            Ur = Ur_old * Wr - Ui * Wi;
            Ui = Ur_old * Wi + Ui * Wr;
        }
    }
}

void defineSine(int len, vector<myFloat>& ar, vector<myFloat>& ai)
{
    // clear
    for (int idx = 0; idx < len; idx++) {
        ai[idx] = 0;
        ar[idx] = 0;
    }

    // sine
    ar[1] = 1.0;
}

void defineTriangle(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai)
{
    if (numHarmonics > (len >> 1))
        numHarmonics = (len >> 1);

    // clear
    for (int idx = 0; idx < len; idx++) {
        ai[idx] = 0;
        ar[idx] = 0;
    }

    // triangle
    float sign = 1;
    for (int idx = 1, jdx = len - 1; idx <= numHarmonics; idx++, jdx--) {
        myFloat temp = idx & 0x01 ? 1.0 / (idx * idx) * (sign = -sign) : 0.0;
        ar[idx] = -temp;
        ar[jdx] = temp;
    }
}

//
// defineSawtooth
//
// prepares sawtooth harmonics for ifft
//
void defineSawtooth(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai)
{
    if (numHarmonics > (len >> 1))
        numHarmonics = (len >> 1);

    // clear
    for (int idx = 0; idx < len; idx++) {
        ai[idx] = 0;
        ar[idx] = 0;
    }

    // sawtooth
    for (int idx = 1, jdx = len - 1; idx <= numHarmonics; idx++, jdx--) {
        myFloat temp = -1.0 / idx;
        ar[idx] = -temp;
        ar[jdx] = temp;
    }
}

void defineSquare(int len, int numHarmonics, vector<myFloat> & ar, vector<myFloat> & ai)
{
    if (numHarmonics > (len >> 1))
        numHarmonics = (len >> 1);

    // clear
    for (int idx = 0; idx < len; idx++) {
        ai[idx] = 0;
        ar[idx] = 0;
    }

    // square
    for (int idx = 1, jdx = len - 1; idx <= numHarmonics; idx++, jdx--) {
        myFloat temp = idx & 0x01 ? 1.0 / idx : 0.0;
        ar[idx] = temp;
        ar[jdx] = -temp;
    }
}