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

#include "MIDI.h"
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
#define maxVoices (8)  // maximum number of oscillators per stack

#define baseFrequency (20)  /* starting frequency of first table */

struct waveTable {
    double topFreq;
    int waveTableLen;
    vector<float> waveTable_;
};

struct perNoteData {
    double mPhasor = randomFloat(0.0, 1.0);       // phase accumulator
    double mPhaseInc = 0.0;     // phase increment
    int mCurWaveTable = 0;      // current table, based on current frequency
};

class WaveTableOsc {
public:
    WaveTableOsc(void) {
        waveTable newTable;
        for (int idx = 0; idx < numWaveTableSlots; idx++) {
            mWaveTables.push_back(newTable);
        }
        perNoteData note;
        for (int p = 0; p < maxPolyphony; p++) {
            notes.push_back(note);
        }
    }
    ~WaveTableOsc(void) {
        mWaveTables.clear();
    }

    //
    // SetFrequency: Set normalized frequency, typically 0-0.5 (must be positive and less than 1!)
    //
    void setFrequency(double inc, int noteIndex) {
            notes[noteIndex].mPhaseInc = inc;

            // update the current wave table selector
            int curWaveTable = 0;
            while ((notes[noteIndex].mPhaseInc >= mWaveTables[curWaveTable].topFreq) && (curWaveTable < (mNumWaveTables - 1))) {
                ++curWaveTable;
            }

            notes[noteIndex].mCurWaveTable = curWaveTable;
    }

    //
    // resetPhase: Reset phase (for retrig)
    //
    void setPhases(float phase) {
        for (auto& note : notes) {
            note.mPhasor = phase;
        }
    }

    void randomisePhases() {
        for (auto& note : notes) {
            note.mPhasor = randomFloat(0.0, 1.0);
        }
    }

    //
    // UpdatePhase: Call once per sample
    //
    void updatePhases(void) {
        for (auto& note : notes) {
            note.mPhasor += note.mPhaseInc;

            if (note.mPhasor >= 1.0)
                note.mPhasor -= 1.0;
        }
    }

    //
    // Process: Update phase and get output
    //
    float process(void) {
        updatePhases();
        return getOutput();
    }

    //
    // GetOutput: Returns the current oscillator output
    //
    float getOutput(void) {
        float out = 0.0;
        for (auto& note : notes) {
            if (note.mPhaseInc != 0.0) {
                // important to define pointer instead of new waveTable for performance!
                waveTable* thisTable = &mWaveTables[note.mCurWaveTable];

                //while (thisTable->waveTable_.size() != 4097 || thisTable->waveTableLen != 4096) {
                //    return 0.0;
                //}

                // linear interpolation
                float temp = note.mPhasor * thisTable->waveTableLen;
                int intPart = temp;
                float fracPart = temp - intPart;

                // wrap to avoid vector subscript out of range with intPart + 1
                if (intPart == thisTable->waveTableLen) {
                    intPart = 0;
                }

                float samp0 = thisTable->waveTable_[intPart];
                float samp1 = thisTable->waveTable_[intPart + 1];
                out += samp0 + (samp1 - samp0) * fracPart;
            }
        }
        return out;
    }

    void setTables(vector<waveTable>* tables) {
        mWaveTables = *tables;
        mNumWaveTables = mWaveTables.size();
    }

    vector<waveTable>* getTables() {
        return &mWaveTables;
    }

    void clearTables() {
        mWaveTables.clear();
    }

protected:
    vector<perNoteData> notes;
    int mNumWaveTables = 0;     // number of wavetable slots in use
    static constexpr int numWaveTableSlots = 40;    // simplify allocation with reasonable maximum
    vector<waveTable> mWaveTables;
};

class WaveTableOscStack {
public:

    WaveTableOscStack() {
        for (int n = 0; n < maxVoices; n++) {
            WaveTableOsc newOsc;
            mOscillators.push_back(newOsc);
        }
        //randomiseAllPhases();
    }

    ~WaveTableOscStack(void) {
        mOscillators.clear();
    }
    
    float processAll() {
        float out = 0.0;

        if (stackOn) {
            for (int n = 0; n < voices; n++) {
                out += mOscillators[n].process();
            }
        }
        return out * amplitude;

        // implement panning spread
    }

    // freqs are already normalised
    void setFrequencies(double freq, int noteIndex) {
        // implement octave/semitone

        for (int n = 0; n < voices; n++) {
            double detuneMultiplier = pow(2, (unisonDetune / 100.0) * allDetuneSemitones[voices - 1][n] / 12.0);

            mOscillators[n].setFrequency(freq * detuneMultiplier, noteIndex);
        }
    }

    void setAllTables(vector<waveTable>* tables) {
        for (auto& mOscillator : mOscillators) {
            mOscillator.setTables(tables);
        }
    }

    void resetAllPhases() {
        for (auto& mOscillator : mOscillators) {
            mOscillator.setPhases(0.0);
        }
    }

    void randomiseAllPhases() {
        for (auto& mOscillator : mOscillators) {
            mOscillator.randomisePhases();
        }
    }

    // parameters that can be directly edited by external processes
    bool    stackOn = true;
    int     shape = 0;
    int     voices = 1;
    float   unisonDetune = 0.0;
    float   unisonSpread = 100.0;
    int     pan = 0;
    float   amplitude = 0.5;

protected:
    vector<WaveTableOsc> mOscillators;

    // spread of each voice (with max detune) in semitones from central note
    vector<vector<double>> allDetuneSemitones = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, -1.0 / 3.0, 1.0 / 3.0, 1.0, 0.0, 0.0, 0.0, 0.0},
    {-1.0, -0.5, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0},
    {-1.0, -0.6, -0.2, 0.2, 0.6, 1.0, 0.0, 0.0},
    {-1.0, -2.0 / 3.0, -1.0 / 3.0, 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0, 0.0},
    {-1.0, -1.0 + 2.0 / 7.0, -1.0 + 4.0 / 7.0, -1.0 + 6.0 / 7.0, 1.0 - 6.0 / 7.0, 1.0 - 4.0 / 7.0, 1.0 - 2.0 / 7.0, 1.0}
    };
};

vector<vector<waveTable>> templateTables(numberOfShapes);

#define maxOscStacks (3)
int numberOfOscStacks = 1;

vector<WaveTableOscStack> oscStacks;

void fft(int N, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSine(int len, vector<myFloat>& ar, vector<myFloat>& ai);
void defineTriangle(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSawtooth(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);
void defineSquare(int len, int numHarmonics, vector<myFloat>& ar, vector<myFloat>& ai);

void makeAllTables(vector<vector<waveTable>>* allTables, float baseFreq);
waveTable makeWaveTable(int len, vector<myFloat>& ar, vector<myFloat>& ai, double topFreq);

#endif
