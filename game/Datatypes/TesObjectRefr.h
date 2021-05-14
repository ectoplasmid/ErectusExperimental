#pragma once
#include <cstdint>

#include "TesItem.h"
#include "../../utils.h"

enum class ActorState
{
	Unknown,
	Alive,
	Dead,
	Downed,
};

class PredictedHealth
{
public:
	DWORD32 entityId; //0x0
	DWORD32 Value; //0x4
};

class ActorSnapshotFurnitureData
{
public:
	BYTE FurnitureMarkerIndex; //0x0
	BYTE FurnitureEntryIndex; //0x1
	BYTE isInteractingWithFurniture; //x02
	BYTE isExitingFurniture; //0x3
	BYTE CullWhileNotInFurniture; //0x4
	BYTE padding0x5[3]; //0x5
	DWORD32 DesiredFurnitureID_; //0x8
	DWORD32 OldDesiredFurnitureID; //0xC
	BYTE DesiredFurnitureIDChanged; //0x10
	BYTE field_F9[3]; //0x11
	DWORD32 OccupiedFurnitureID; //0x14
};

class ActorSnapshotComponentBase
{
public:
	//ActorCoreSnapshotComponent
	DWORD64 actorCorevtable; //0x0
	BYTE actorCorePadding0008[0x98];
	//ActorServerAuthSnapshotData    // 0x38 isInvulnerable, 0x3C isProtected, 0x3D IsPlayerProtected, 0x130 hostileState, 0x138 reconScopeTargetState
};

class ActorSnapshotComponent : public ActorSnapshotComponentBase
{
public:
	DWORD64 vtable; //0x00
	DWORD64 builtByID; //0x08
	BYTE keywordFlags[0x10]; //0x10
	DWORD32 actorLifeState; //0x20
	float vatsCriticalCharge; //0x24
	DWORD32 vatsCriticalCount; //0x28
	BYTE Disabled; // 0x2C
	BYTE DisabledPrev; // 0x2D
	BYTE DisabledChanged; // 0x2E
	BYTE DeathFade; // 0x2F
	BYTE DisableFade; // 0x30
	BYTE Powered; //0x31
	BYTE InCombat; //0x32
	BYTE ignoreCombat; //0x33
	BYTE isEaten; //0x34
	BYTE hasDeferredLegendaryDrop; //0x35
	BYTE processMe; //0x36
	BYTE isBoss; //0x37
	BYTE isInvulnerable; //0x38
	BYTE isGhost; //0x39
	BYTE isGhostForTeammate; //0x3A
	BYTE isEssential; //0x3B
	BYTE isProtected; //0x3C
	BYTE isPlayerProtected; //0x3D
	WORD normalizedMaxLevel; //0x3E
	WORD normalizedMinLevel; //0x40
	BYTE normalizedLevelOffset; //0x42
	BYTE padding0x43[0x35];
	float maxHealth; //0x78
	float modifiedHealth; //0x7C
	float healthValue2; //0x80
	float lostHealth; //0x84
	BYTE padding0x88[0x1C]; //0x88-0xA3
	PredictedHealth PredictedHealthValues[8]; //0xA4 - 0xE3
	DWORD32 padding0xE4; //0xE4
	ActorSnapshotFurnitureData CurrentFurnitureData; //0xE8
	ActorSnapshotFurnitureData PreviousFurnitureData; //0x100
	BYTE FurnitureDataChanged; //0x118
	BYTE padding0x119[7]; //0x119
	DWORD64 ActorRace; //0x120
	BYTE epicRank; //0x128
	BYTE padding0129[7]; //0x129
	DWORD64 VoiceType; //0x130
	DWORD64 hostileState; //0x138
	DWORD64 reconScopeTargetState; //0x140
	DWORD32 QuestGhostedInstanceIds[2]; //0x148,0x14C
	DWORD32 TeleportFadeStateEnum; //0x150
	BYTE padding0x153[4]; //0x154
};
class Inventory
{
public:
	std::uintptr_t vtable;//0x0
	char padding0008[0x58];
	std::uintptr_t entryArrayBegin;//0x60
	std::uintptr_t entryArrayEnd;//0x68
};

class InventoryEntry
{
public:
	std::uintptr_t baseObjectPtr;//0x0
	std::uintptr_t instancePtr;
	std::uintptr_t displayPtr;//0x10
	char padding0018[0x8];
	std::uintptr_t iterations;//0x20
	char equipFlag;//0x28
	char padding0025[0x3];
	std::uint32_t itemId;//0x2C
	char favoriteIndex;//0x30
	char padding0031[0x7];
};

class TesObjectRefr
{
public:
	[[nodiscard]] FormType GetFormType() const { return static_cast<FormType>(formType); }
	[[nodiscard]] TesItem GetBaseObject() const;
	[[nodiscard]] ActorState GetActorState() const;

	[[nodiscard]] bool IsEssential() const { return GetActorSnapshot().isEssential; }
	[[nodiscard]] bool IsHostile() const { return GetActorSnapshot().hostileState != 0; }

	[[nodiscard]] std::uint8_t GetEpicRank() const { return GetActorSnapshot().epicRank; }

	[[nodiscard]] float GetCurrentHealth() const
	{
		const auto snapshot = GetActorSnapshot();
		return snapshot.maxHealth + snapshot.modifiedHealth + snapshot.lostHealth;
	}

	[[nodiscard]] std::vector<InventoryEntry> GetInventory() const;

private:
	[[nodiscard]] ActorSnapshotComponent GetActorSnapshot() const;

public:
	std::uintptr_t vtable; //0x0
	char padding0008[0x8];
	char harvestFlagA; //0x10
	char padding0011[0x8];
	char harvestFlagB; //0x19
	char padding001A[0x6];
	std::uint32_t formId; //0x20
	char padding0024[0x2]; //0x24
	std::uint8_t formType; //0x26
	char padding0027[0x11]; //0x27
	char idValue[4]; //0x38
	char padding003C[0x14];
	std::uintptr_t vtable0050; //0x50
	char padding0058[0x8];
	float pitch; //0x60
	char padding0064[0x4];
	float yaw; //0x68
	char padding006C[0x4];
	Vector3 position; //0x70
	char padding007C[0x4];
	std::uintptr_t inventoryPtr; //0x80
	char padding0088[0x8];
	std::uintptr_t actorCorePtr; //0x90
	char padding0098[0x10];
	std::uintptr_t cellPtr; //0xA8
	std::uintptr_t skeletonPtr; //0xB0
	std::uintptr_t baseObjectPtr; //0xB8
	char padding00C0[0xE];
	char spawnFlag; //0xCE
	char padding00Cf[0xD9];
	char movementFlag; //0x1A8
	char sprintFlag; //0x1A9
	char healthFlag; //0x1AA
	char padding019B[0xAA9];
	std::uint32_t playerStashFormId; //0xC54
	char padding0C58[0x140];
	std::uintptr_t playerKnownRecipes;

	std::uintptr_t ptr; //not really there, fill it manually
};

