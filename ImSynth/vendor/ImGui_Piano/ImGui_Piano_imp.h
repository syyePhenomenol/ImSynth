/*
The MIT License (MIT)
by https://github.com/frowrik

Piano keyboard for ImGui v1.1

example:
static int PrevNoteActive = -1;
ImGui_PianoKeyboard("PianoTest", ImVec2(1024, 100), &PrevNoteActive, 21, 108, TestPianoBoardFunct, nullptr, nullptr);

bool TestPianoBoardFunct(void* UserData, int Msg, int Key, float Vel) {
		if (Key >= 128) return false; // midi max keys
		if (Msg == NoteGetStatus) return KeyPresed[Key];
		if (Msg == NoteOn) { KeyPresed[Key] = true; Send_Midi_NoteOn(Key, Vel*127); }
		if (Msg == NoteOff) { KeyPresed[Key] = false; Send_Midi_NoteOff(Key, Vel*127);}
		return false;
}

*/
#include "imgui.h"
#include "imgui_internal.h"
#include "MIDI.h"

#include <iostream>
#include <map>

using namespace std;

int keyboardOctave = 4;
int keyboardBaseKey = 96;

map <char, int> keyCharToKey {
	{'Z', 0},
	{'S', 1},
	{'X', 2},
	{'D', 3},
	{'C', 4},
	{'V', 5},
	{'G', 6},
	{'B', 7},
	{'H', 8},
	{'N', 9},
	{'J', 10},
	{'M', 11},
	{',', 12},
	{'L', 13},
	{'.', 14},
	{';', 15},
	{'/', 16},
	{'Q', 12},
	{'2', 13},
	{'W', 14},
	{'3', 15},
	{'E', 16},
	{'R', 17},
	{'5', 18},
	{'T', 19},
	{'6', 20},
	{'Y', 21},
	{'7', 22},
	{'U', 23},
	{'I', 24},
	{'9', 25},
	{'O', 26},
	{'0', 27},
	{'P', 28}
};

enum ImGuiPianoKeyboardMsg {
	//NoteGetStatus,
	NoteOn,
	NoteOff,
};

using ImGuiPianoKeyboardProc = bool (*)(void* UserData, int Msg, int Key, float Vel);

using freqHandler = void (*)(int Key, int KeyIndex);

struct ImGuiPianoStyles {
	ImU32 Colors[5]{
		IM_COL32(255, 255, 255, 255),	// light note
		IM_COL32(0, 0, 0, 255),			// dark note
		IM_COL32(160, 189, 248, 255),	// active light note
		IM_COL32(42, 72, 131, 255),	// active dark note
		IM_COL32(75, 75, 75, 255),		// background
	};
	float NoteDarkHeight = 2.0f / 3.0f; // dark note scale h
	float NoteDarkWidth  = 2.0f / 3.0f;	// dark note scale w
};

bool pianoCallback(freqHandler Callback, int Msg, int Key, float Vel)
{
	if ((Key > 108) || (Key < 21)) return false; // midi max keys
	//if (Msg == NoteGetStatus) return keyPressed[Key];
	if (Msg == NoteOn) {
		// if this key is not in the buffer
		if (findKeyInBuffer(Key) == maxPolyphony) {
			// if there is space in the buffer
			int slot = findKeyInBuffer(midiNone);
			if (slot != maxPolyphony) {
				Callback(Key, slot);
				keyBuffer[slot] = Key;
			}
		}
	}
	if (Msg == NoteOff) {
		// if the key is in the buffer
		int slot = findKeyInBuffer(Key);
		if (slot != maxPolyphony) {
			Callback(midiNone, slot);
			keyBuffer[slot] = midiNone;
		}
	}
	return false;
}

void ImGui_PianoKeyboard(const char* IDName, ImVec2 Size, int* PrevActiveNote, int BeginOctaveNote, int EndOctaveNote, freqHandler pushFreqs, void* UserData, ImGuiPianoStyles* Style = nullptr) {
	// const
	static int NoteIsDark[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
	static int NoteLightNumber[12] = { 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6, 7 };
	static float NoteDarkOffset[12] = { 0.0f,  -2.0f / 3.0f, 0.0f, -1.0f / 3.0f, 0.0f, 0.0f, -2.0f / 3.0f, 0.0f, -0.5f, 0.0f, -1.0f / 3.0f, 0.0f };

	// fix range dark keys
	if (NoteIsDark[BeginOctaveNote % 12] > 0) BeginOctaveNote++;
	if (NoteIsDark[EndOctaveNote % 12] > 0) EndOctaveNote--;

	// bad range
	if (!IDName || !pushFreqs || BeginOctaveNote < 0 || EndOctaveNote < 0 || EndOctaveNote <= BeginOctaveNote) return;

	// style
	static ImGuiPianoStyles ColorsBase;
	if (!Style) Style = &ColorsBase;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return;

	const ImGuiID id = window->GetID(IDName);

	ImDrawList* draw_list = window->DrawList;

	ImVec2 Pos = window->DC.CursorPos;
	ImVec2 MousePos = ImGui::GetIO().MousePos;

	// sizes
	int CountNotesAllign7 = (EndOctaveNote / 12 - BeginOctaveNote / 12) * 7 + NoteLightNumber[EndOctaveNote % 12] - (NoteLightNumber[BeginOctaveNote % 12] - 1);

	float NoteHeight = Size.y;
	float NoteWidth = Size.x / (float)CountNotesAllign7;

	float NoteHeight2 = NoteHeight * Style->NoteDarkHeight;
	float NoteWidth2 = NoteWidth * Style->NoteDarkWidth;

	// minimal size draw
	if (NoteHeight < 5.0 || NoteWidth < 3.0) return;

	// minimal size using mouse
	bool isMouseInput = (NoteHeight >= 10.0 && NoteWidth >= 5.0);

	// item
	const ImRect bb(Pos.x, Pos.y, Pos.x + Size.x, Pos.y + Size.y);
	ImGui::ItemSize(Size, 0);
	if (!ImGui::ItemAdd(bb, id)) return;

	// item input
	bool held = false;
	if (isMouseInput) {
		ImGui::ButtonBehavior(bb, id, nullptr, &held, 0);
	}

	int		NoteMouseCollision = -1;
	float	NoteMouseVel = 0.0f;

	float OffsetX = bb.Min.x;
	float OffsetY = bb.Min.y;
	float OffsetY2 = OffsetY + NoteHeight;
	for (int RealNum = BeginOctaveNote; RealNum <= EndOctaveNote; RealNum++) {
		int Octave = RealNum / 12;
		int i = RealNum % 12;

		if (NoteIsDark[i] > 0) continue;

		ImRect NoteRect(
			round(OffsetX),
			OffsetY,
			round(OffsetX + NoteWidth),
			OffsetY2
		);

		if (held && NoteRect.Contains(MousePos)) {
			NoteMouseCollision = RealNum;
			NoteMouseVel = (MousePos.y - NoteRect.Min.y) / NoteHeight;
		}

		//bool isActive = Callback(UserData, NoteGetStatus, RealNum, 0.0f);

		draw_list->AddRectFilled(NoteRect.Min, NoteRect.Max, Style->Colors[(findKeyInBuffer(RealNum) != maxPolyphony) ? 2 : 0], 0.0f);

		draw_list->AddRect(NoteRect.Min, NoteRect.Max, Style->Colors[4], 0.0f);

		OffsetX += NoteWidth;
	}

	// draw dark notes
	OffsetX = bb.Min.x;
	OffsetY = bb.Min.y;
	OffsetY2 = OffsetY + NoteHeight2;
	for (int RealNum = BeginOctaveNote; RealNum <= EndOctaveNote; RealNum++) {
		int Octave = RealNum / 12;
		int i = RealNum % 12;

		if (NoteIsDark[i] == 0) {
			OffsetX += NoteWidth;
			continue;
		}

		float OffsetDark = NoteDarkOffset[i] * NoteWidth2;
		ImRect NoteRect(
			round(OffsetX + OffsetDark),
			OffsetY,
			round(OffsetX + NoteWidth2 + OffsetDark),
			OffsetY2
		);

		if (held && NoteRect.Contains(MousePos)) {
			NoteMouseCollision = RealNum;
			NoteMouseVel = (MousePos.y - NoteRect.Min.y) / NoteHeight2;
		}

		//bool isActive = Callback(UserData, NoteGetStatus, RealNum, 0.0f);

		draw_list->AddRectFilled(NoteRect.Min, NoteRect.Max, Style->Colors[(findKeyInBuffer(RealNum) != maxPolyphony) ? 3 : 1], 0.0f);

		draw_list->AddRect(NoteRect.Min, NoteRect.Max, Style->Colors[4], 0.0f);
	}

	static int prevMouseNote = midiNone;

	// mouse input
	if (prevMouseNote != NoteMouseCollision) {
		pianoCallback(pushFreqs, NoteOff, prevMouseNote, 0.0f);
		prevMouseNote = 128;

		if (held && NoteMouseCollision >= 0) {
			pianoCallback(pushFreqs, NoteOn, NoteMouseCollision, NoteMouseVel);
			prevMouseNote = NoteMouseCollision;
		}
	}

	// key input - playing the piano keyboard
	for (auto& key : keyCharToKey)
	{
		char thisChar = key.first;
		int thisKey = key.second + keyboardOctave * 12;

		if (ImGui::IsKeyPressed(thisChar)) {
			pianoCallback(pushFreqs, NoteOn, thisKey, 0.75);
		}
		if (ImGui::IsKeyReleased(thisChar)) {
			pianoCallback(pushFreqs, NoteOff, thisKey, 0.75);
		}
	}
	
	// key input - octave control
	if (ImGui::IsKeyPressed('=') && keyboardOctave < 8) {
		for (int n = 0; n < maxPolyphony; n++) {
			pushFreqs(midiNone, n);
			keyBuffer[n] = midiNone;
		}
		keyboardOctave++;
	} else if (ImGui::IsKeyPressed('-') && keyboardOctave > 0) {
		for (int n = 0; n < maxPolyphony; n++) {
			pushFreqs(midiNone, n);
			keyBuffer[n] = midiNone;
		}
		keyboardOctave--;
	}

	/*ImGuiIO &io = ImGui::GetIO();
	ImGui::Text("Keys down:");          for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++) if (ImGui::IsKeyDown(i)) { ImGui::SameLine(); ImGui::Text("%d (0x%X) (%.02f secs)", i, i, io.KeysDownDuration[i]); }*/
}