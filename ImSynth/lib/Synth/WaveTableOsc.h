//
//  WaveTableOsc.h
//
//  Created by Nigel Redmon on 2018-10-05
//  EarLevel Engineering: earlevel.com
//  Copyright 2018 Nigel Redmon
//
//  For a complete explanation of the wavetable oscillator and code,
//  read the series of articles by the author, starting here:
//  www.earlevel.com/main/2012/05/03/a-wavetable-oscillator—introduction/
//
//  This version has optimizations described here:
//  www.earlevel.com/main/2019/04/28/wavetableosc-optimized/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//

#ifndef WaveTableOsc_h
#define WaveTableOsc_h

#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

#include "randomFloat.h"

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

//
// some things to tweak for testing and experimenting
//
#define sampleRate (44100)

// oscillator
#define overSamp (2)        /* oversampling factor (positive integer) */
#define baseFrequency (20)  /* starting frequency of first table */
#define constantRatioLimit (99999)    /* set to a large number (greater than or equal to the length of the lowest octave table) for constant table size; set to 0 for a constant oversampling ratio (each higher ocatave table length reduced by half); set somewhere between (64, for instance) for constant oversampling but with a minimum limit */

#define myFloat double      /* float or double, to set the resolution of the FFT, etc. (the resulting wavetables are always float) */

#define numberOfShapes (4) // total number of wave shapes implemented
#define maxUnisonVoices (8)  // maximum number of oscillators per stack

struct waveTable {
    double topFreq;
    int waveTableLen;
    vector<float> waveTable_;
};

class WaveTableOsc {
public:
    WaveTableOsc(void) {
        waveTable newTable;
        for (int idx = 0; idx < numWaveTableSlots; idx++) {
            mWaveTables.push_back(newTable);
        }
    }
    ~WaveTableOsc(void) {
        mWaveTables.clear();
    }

    //
    // SetFrequency: Set normalized frequency, typically 0-0.5 (must be positive and less than 1!)
    //
    void setFrequency(double inc) {
        mPhaseInc = inc;

        // update the current wave table selector
        int curWaveTable = 0;
        while ((mPhaseInc >= mWaveTables[curWaveTable].topFreq) && (curWaveTable < (mNumWaveTables - 1))) {
            ++curWaveTable;
        }
        mCurWaveTable = curWaveTable;
    }

    //
    // SetPhaseOffset: Phase offset for PWM, 0-1
    //
    void setPhaseOffset(double offset) {
        mPhaseOfs = offset;
    }

    //
    // resetPhase: Reset phase (for retrig)
    //
    void setPhase(float phase) {
        mPhasor = phase;
    }

    //
    // UpdatePhase: Call once per sample
    //
    void updatePhase(void) {
        mPhasor += mPhaseInc;

        if (mPhasor >= 1.0)
            mPhasor -= 1.0;
    }

    //
    // Process: Update phase and get output
    //
    float process(void) {
        updatePhase();
        return getOutput();
    }

    //
    // GetOutput: Returns the current oscillator output
    //
    float getOutput(void) {
        waveTable* thisTable = &mWaveTables[mCurWaveTable];

        // linear interpolation
        float temp = mPhasor * thisTable->waveTableLen;
        int intPart = temp;
        float fracPart = temp - intPart;

        // wrap to avoid vector subscript out of range with intPart + 1
        if (intPart == thisTable->waveTableLen) {
            intPart = 0;
        }

        float samp0 = thisTable->waveTable_[intPart];
        float samp1 = thisTable->waveTable_[intPart + 1];
        return samp0 + (samp1 - samp0) * fracPart;
    }

    //
    // getOutputMinusOffset
    //
    // for variable pulse width: initialize to sawtooth,
    // set phaseOfs to duty cycle, use this for osc output
    //
    // returns the current oscillator output
    //
    float getOutputMinusOffset() {
        waveTable* thisTable = &mWaveTables[mCurWaveTable];
        int len = thisTable->waveTableLen;
        vector<float> wave = thisTable->waveTable_;

        // linear
        float temp = mPhasor * len;
        int intPart = temp;
        float fracPart = temp - intPart;
        float samp0 = wave[intPart % thisTable->waveTableLen];
        float samp1 = wave[(intPart + 1) % thisTable->waveTableLen];
        float samp = samp0 + (samp1 - samp0) * fracPart;

        // and linear again for the offset part
        float offsetPhasor = mPhasor + mPhaseOfs;
        if (offsetPhasor > 1.0)
            offsetPhasor -= 1.0;
        temp = offsetPhasor * len;
        intPart = temp;
        fracPart = temp - intPart;
        samp0 = wave[intPart];
        samp1 = wave[intPart + 1];
        return samp - (samp0 + (samp1 - samp0) * fracPart);
    }

    ////
    //// AddWaveTable
    ////
    //// add wavetables in order of lowest frequency to highest
    //// topFreq is the highest frequency supported by a wavetable
    //// wavetables within an oscillator can be different lengths
    ////
    //// returns 0 upon success, or the number of wavetables if no more room is available
    ////
    //int addWaveTable(int len, vector<float>& waveTableIn, double topFreq, int tableIndex) {
    //    if (tableIndex < numWaveTableSlots) {
    //        mNumWaveTables = tableIndex;
    //        waveTable* newTable = new waveTable;
    //        newTable->waveTableLen = len;
    //        newTable->topFreq = topFreq;
    //        newTable->waveTable_ = waveTableIn;
    //        newTable->waveTable_.push_back(waveTableIn.front());

    //        mWaveTables[tableIndex] = *newTable;
    //        return 0;
    //    }
    //    return numWaveTableSlots;
    //}

    void setTables(vector<waveTable>* tables) {
        mWaveTables = *tables;
        mNumWaveTables = tables->size();
    }

    vector<waveTable>* getTables() {
        return &mWaveTables;
    }

    void clearTables() {
        mWaveTables.clear();
    }

protected:
    double mPhasor = 0.0;       // phase accumulator
    double mPhaseInc = 0.0;     // phase increment
    double mPhaseOfs = 0.5;     // phase offset for PWM

    // array of wavetables
    int mCurWaveTable = 0;      // current table, based on current frequency
    int mNumWaveTables = 0;     // number of wavetable slots in use
    static constexpr int numWaveTableSlots = 40;    // simplify allocation with reasonable maximum
    vector<waveTable> mWaveTables;
};

class WaveTableOscStack {
public:
    //WaveTableOscStack(WaveTableOsc* osc, int shape) {
    //    for (int n = 0; n < maxUnisonVoices; n++) {
    //        mOscillators.push_back(*osc);
    //    }
    //}

    WaveTableOscStack() {
        for (int n = 0; n < maxUnisonVoices; n++) {
            WaveTableOsc newOsc;
            mOscillators.push_back(newOsc);
        }
    }

    ~WaveTableOscStack(void) {
        mOscillators.clear();
    }
    
    float processAll() {
        float out = 0.0;

        if (stackOn) {
            for (int n = 0; n < unisonVoices; n++) {
                out += mOscillators[n].process();
            }
        }
        return out * amplitude;

        // implement panning spread
    }

    void setAmplitude(double amp) {
        amplitude = amp;
    }

    // freq is already normalised
    void setFrequencies(double freq) {
        // need to add in detune stuff
        // implement octave/semitone

        for (int n = 0; n < unisonVoices; n++) {
            float finalDetuneMultiplier = pow(2, (unisonDetune / 100.0) * detuneSemitones[n] / 12.0);
            mOscillators[n].setFrequency(freq * finalDetuneMultiplier);
            //cout << detunedFreq << endl;
        }
    }

    void setTables(vector<waveTable>* tables) {
        for (int n = 0; n < unisonVoices; n++) {
            mOscillators[n].setTables(tables);
        }
        //stackShape = shape;
    }

    void setShapes(WaveTableOsc* osc) {
        for (int n = 0; n < unisonVoices; n++) {
            mOscillators[n] = *osc;
        }
        //stackShape = shape;
    }

    // copy first oscillator's tables into every other oscillator required
    void setVoices (vector<double>* detune) {
        vector<waveTable>* tables = mOscillators.front().getTables();
        for (int n = 1; n < unisonVoices; n++) {
            mOscillators[n].setTables(tables);
        }

        // update detuneMultipliers
        detuneSemitones = *detune;
    }

    void resetPhases() {
        for (int n = 1; n < unisonVoices; n++) {
            mOscillators[n].setPhase(0.0);
        }
    }

    void randomisePhases() {
        for (int n = 1; n < unisonVoices; n++) {
            mOscillators[n].setPhase(randomFloat(0.0, 1.0));
        }
    }

    // parameters that can be directly edited by external processes
    bool    stackOn = true;
    int     shape = 0;
    int     unisonVoices = 1;
    float   unisonSpread = 100.0;
    float   unisonDetune = 0.0;
    int     pan = 0;
    float   amplitude = 0.5;

protected:
    vector<WaveTableOsc> mOscillators;
    vector<double> detuneSemitones{ 0.0 }; // updated when number of voices changes
    double  freq = 0.0;
    int     octave = 0;
};

void fft(int N, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSine(int len, vector<myFloat>& ar, vector<myFloat>& ai);
void defineTriangle(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSawtooth(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSquare(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);
//float makeWaveTable(WaveTableOsc* osc, int len, vector<myFloat>& ar, vector<myFloat>& ai, myFloat scale, double topFreq, int tableIndex);
//void setOsc(WaveTableOsc* osc, float baseFreq, int waveShape);

void makeAllTables(vector<vector<waveTable>>* allTables, float baseFreq);
waveTable makeWaveTable(int len, vector<myFloat>& ar, vector<myFloat>& ai, double topFreq);

#endif
