#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include <stdio.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

#include "portaudio.h"
#include "MIDI.h"
#include "WaveTableOsc.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "gl3w.h"
#include "glfw3.h"

#include "ImGui_Piano_imp.h"
#include "implot.h"
#include "implot_internal.h"

using namespace std;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#define SAMPLE_RATE   (44100)
#define SAMPLE_BUFFER_SIZE   (256)

#ifndef M_PI
    #define M_PI  (3.14159265)
#endif

#define baseFrequency (20)  /* starting frequency of first table */

int numberOfOscillators = 3;

vector<WaveTableOsc> oscillators(numberOfOscillators);

struct oscParameters {
    int Shape = 0;
    float Amplitude = 1.0;
};

vector<oscParameters> allOscParameters(numberOfOscillators);

float detune = 0.0;
float detuneFactor = 1.0;

bool soundOn = false;
float gAmplitude = 0.05f;
unsigned int audioOutChannels = 2;

static vector<float> midiPitches;
bool holdNote = false;
bool retrig = false;
bool keyPressed[128] = { 0 };

vector<float> initMidiPitches()
{
    vector<float> pitches(128, 0.0);
    for (int n = 0; n < 128; n++) {
        pitches[n] = 440 * pow(2, (float(n) - 69.0) / 12.0);
    }
    return pitches;
}

bool pianoCallback(void* UserData, int Msg, int Key, float Vel)
{
    if ((Key >= 128) || (Key < 0)) return false; // midi max keys
    if (Msg == NoteGetStatus) return keyPressed[Key];
    //if (Msg == NoteOn) { KeyPressed[Key] = true; Send_Midi_NoteOn(Key, Vel * 127); }
    //if (Msg == NoteOff) { KeyPressed[Key] = false; Send_Midi_NoteOff(Key, Vel * 127); }
    if (Msg == NoteOn) {
        keyPressed[Key] = true; soundOn = true;
        double freq = midiPitches[Key];
        oscillators[0].setFrequency(freq / SAMPLE_RATE);
        oscillators[1].setFrequency(freq * detuneFactor / SAMPLE_RATE);
        oscillators[2].setFrequency(freq / detuneFactor / SAMPLE_RATE);
        if (retrig) {
            oscillators[0].resetPhase();
            oscillators[1].resetPhase();
            oscillators[2].resetPhase();
        }
    }
    if (Msg == NoteOff) {
        keyPressed[Key] = false;
        if (holdNote) {
            soundOn = true;
        }
        else {
            soundOn = false;
        }
    }
    return false;
}

// Oscilloscope stuff
int scopeBufferSize = SAMPLE_BUFFER_SIZE;
vector<double> scopeIndex(scopeBufferSize, 0.0f);
vector<double> scopeBuffer(scopeBufferSize, 0.0f);
int scopePointer = 0;

static int Render_Audio(const void* inputBuffer,
    void* outputBuffer,
    unsigned long                   framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags           statusFlags,
    void* userData)
{
    float* out = (float*)outputBuffer;
    (void)inputBuffer; /* Prevent unused argument warning. */

    for (unsigned int n = 0; n < framesPerBuffer; n++) {
        double sample = 0.0;

        if (soundOn) {
            sample = oscillators[0].process() * allOscParameters[0].Amplitude
                + oscillators[1].process() * allOscParameters[1].Amplitude
                + oscillators[2].process() * allOscParameters[2].Amplitude;
            sample *= gAmplitude;
        } else {
            sample = 0.0;
        }
        scopeBuffer[scopePointer] = sample;
        scopePointer = (scopePointer + 1) % scopeBufferSize;

        for (unsigned int channel = 0; channel < audioOutChannels; channel++) {
            *out++ = sample;
        }
    }
    return 0;
}

int main(void)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    const char* glsl_version = "#version 130";
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImSynth pre-alpha", NULL, NULL);
    if (window == NULL) return 1;
    glfwSetWindowSizeLimits(window, 1080, 720, 1080, 720);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    bool OpenGLerr = gl3wInit() != 0;
    if (OpenGLerr) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    bool showOscillators = true;
    bool showGlobal = true;
    bool showKeyboard = true;
    bool showOscilloscope = true;
    ImVec4 clear_color = ImVec4(0.24f, 0.35f, 0.56f, 1.00f);

    // Initialise oscillators
    setOsc(&oscillators[0], baseFrequency, 0);
    setOsc(&oscillators[1], baseFrequency, 0);
    setOsc(&oscillators[2], baseFrequency, 0);

    midiPitches = initMidiPitches();

    iota(begin(scopeIndex), end(scopeIndex), 0);

    PaStreamParameters outputParameters;
    PaStream* stream;
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        goto error;
    }

    outputParameters.channelCount = audioOutChannels;                     /* stereo output */
    outputParameters.sampleFormat = paFloat32;             /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&stream,
        NULL,                   /* No input. */
        &outputParameters,      /* As above. */
        SAMPLE_RATE,
        SAMPLE_BUFFER_SIZE,     /* Frames per buffer. */
        paClipOff,              /* No out of rang8e samples expected. */
        Render_Audio,
        0);
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        static int prevNoteActive = 128; // 128 = None
        static int prevNote = 128;

        if (prevNoteActive != 128) {
            prevNote = prevNoteActive;
        }

        if (!holdNote && prevNoteActive == 128) {
            soundOn = false;
        }

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Show main control window.
        {
            ImGui::Begin("Widget settings", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Checkbox("Oscillators", &showOscillators);
            ImGui::Checkbox("Keyboard", &showKeyboard);
            ImGui::Checkbox("Oscilloscope", &showOscilloscope);
            ImGui::Checkbox("Global", &showGlobal);

            ImGui::End();
        }

        if (showOscillators) {
            ImGui::Begin("Oscillators", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            const char* waveShape[] = {"Sine", "Triangle", "Saw", "Square" };

            for (int n = 0; n < numberOfOscillators; n++) {
                ImGui::PushID(n);

                ImGui::Text("Oscillator %u", n);
                if (ImGui::Combo("Wave shape", &allOscParameters[n].Shape, waveShape, IM_ARRAYSIZE(waveShape))) {
                    setOsc(&oscillators[n], baseFrequency, allOscParameters[n].Shape);
                }
                ImGui::SliderFloat("Amplitude", &allOscParameters[n].Amplitude, 0.0, 1.0);

                ImGui::PopID();
            }

            ImGui::TextUnformatted("");
            ImGui::Checkbox("Retrig", &retrig);
            if (ImGui::SliderFloat("Detune", &detune, 0.0, 100.0)) {
                detuneFactor = pow(2, (detune / 100.0) / 12.0);
                double freq = midiPitches[prevNote];
                oscillators[1].setFrequency(freq * detuneFactor / SAMPLE_RATE);
                oscillators[2].setFrequency(freq / detuneFactor / SAMPLE_RATE);
            }
            //ImGui::Text("Detune factor: %.3f", detuneFactor);
            ImGui::End();
        }

        if (showKeyboard) {
            ImGui::Begin("Piano keyboard", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui_PianoKeyboard("PianoTest", ImVec2(1060, 120), &prevNoteActive, 21, 108, pianoCallback, nullptr, nullptr);

            ImGui::End();
        }

        if (showOscilloscope) {
            ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_Once);

            if (ImGui::Begin("Oscilloscope")) { // needed otherwise will crash upon minimising

                ImPlot::SetNextPlotLimitsX(0.0, (float)SAMPLE_BUFFER_SIZE - 1.0, ImGuiCond_Always);
                ImPlot::SetNextPlotLimitsY(-gAmplitude * 3, gAmplitude * 3, ImGuiCond_Always);
                ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, -1), 3.0f);
                if (ImPlot::BeginPlot("", 0, 0, ImVec2(-1, -1), ImPlotFlags_AntiAliased, ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels,
                    ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels)) {
                    ImPlot::PlotLine("", &scopeIndex[0], &scopeBuffer[0], SAMPLE_BUFFER_SIZE);
                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        if (showGlobal) {
            ImGui::Begin("Global", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Checkbox("Hold note", &holdNote);
            ImGui::SliderFloat("Amplitude", &gAmplitude, 0.0, 0.1);
            if (holdNote) {
                ImGui::Text("Active note: %s", &(MIDI_number_to_name[prevNote])[0]);
            } else {
                ImGui::Text("Active note: %s", &(MIDI_number_to_name[prevNoteActive])[0]);
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error;
    Pa_Terminate();

    return err;

    error:
        Pa_Terminate();
        fprintf(stderr, "An error occurred while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        return err;
}
