#include "Hooks_Gameplay.h"
#include "SafeWrite.h"

static UInt32 g_forceContainerCategorization = 0;
static const UInt32 kHook_ContainerMode_Categories = 0x0083A19E;
static const UInt32 kHook_ContainerMode_NoCategories = 0x0083A1B3;
static UInt32 ** g_containerMode = (UInt32 **)0x0138D7A4;

static void __declspec(naked) Hook_ContainerMode(void)
{
	__asm
	{
		// check override
		cmp		g_forceContainerCategorization, 0
		jnz		useCategories

		// original code (modified because msvc doesn't like immediates)
		// ### todo: test if we can just set g_containerMode to 3 and continue normally

		push	eax
		mov		eax, [g_containerMode]
		mov		eax, [eax]
		cmp		eax, 3
		pop		eax

		jz		useCategories

		jmp		[kHook_ContainerMode_NoCategories]

useCategories:
		jmp		[kHook_ContainerMode_Categories]
	}
}

void Hooks_Gameplay_EnableForceContainerCategorization(bool enable)
{
	g_forceContainerCategorization = enable ? 1 : 0;
}

void Hooks_Gameplay_Commit(void)
{
	// optionally force containers in to "npc" mode, showing categories
	WriteRelJump(0x0083A195, (UInt32)Hook_ContainerMode);
}
