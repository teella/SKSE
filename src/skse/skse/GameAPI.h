#pragma once

#include "skse/Utilities.h"

class PlayerCharacter;

// it seems like there are multiple buckets this time - investigate
// move this in to GameMemory.h or something like that if further investigated
class Heap
{
public:
	MEMBER_FN_PREFIX(Heap);
	// haven't verified alignment vars
	DEFINE_MEMBER_FN(Allocate, void *, 0x00A2D680, UInt32 size, UInt32 alignment, bool aligned);
	DEFINE_MEMBER_FN(Free, void, 0x00A2D0D0, void * buf, bool aligned);
};

extern Heap * g_formHeap;

void * FormHeap_Allocate(UInt32 size);
void FormHeap_Free(void * ptr);

extern PlayerCharacter ** g_thePlayer;

void Console_Print(const char * fmt, ...);
bool IsConsoleMode(void);

// 9C
// ControlMap?
// right now we care about this for the UI allowTextInput so it's called InputManager
class InputManager
{
public:
	UInt8	pad00[0x98];	// 00
	UInt8	allowTextInput;	// 98
	UInt8	pad99[3];		// 99

	static InputManager *	GetSingleton(void);

	UInt8	AllowTextInput(bool allow);
};
