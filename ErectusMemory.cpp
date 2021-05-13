#include <array>
#include <set>
#include <string>
#include <memory>

#include "ErectusMemory.h"
#include "CrcCalculator.h"
#include "common.h"
#include "settings.h"
#include "renderer.h"
#include "utils.h"

#include "ErectusProcess.h"
#include "dependencies/fmt/fmt/format.h"
#include "features/MsgSender.h"
#include "game/Game.h"
#include <span>

std::uintptr_t ErectusMemory::GetAddress(const std::uint32_t formId)
{
	std::uintptr_t v1;
	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_GET_PTR_A1, &v1, sizeof v1)) return 0;
	if (!Utils::Valid(v1)) return 0;

	std::uint32_t listMaxSize;
	if (!ErectusProcess::Rpm(v1 + 32, &listMaxSize, sizeof listMaxSize)) return 0;
	if (!listMaxSize) return 0;

	auto formIdCRC = CRCCalculator::CRC32(formId);
	auto v6 = (formIdCRC & (listMaxSize - 1));
	
	std::uintptr_t v7;
	if (!ErectusProcess::Rpm(v1 + 0x18, &v7, sizeof v7)) return 0;
	if (!Utils::Valid(v7)) return 0;

	std::uint32_t v8;
	if (!ErectusProcess::Rpm(v7 + (v6 + v6 * 2) * 0x8 + 0x10, &v8, sizeof(v8))) return 0;		
	if (v8 == 0xFFFFFFFF) return 0;

	auto listIterator = listMaxSize;


	for (int i = 0; i < 100; i++)
	{
		std::uint32_t v10;
		if (!ErectusProcess::Rpm(v7 + (v6 + v6 * 2) * 0x8, &v10, sizeof(v10))) return 0;
		if (v10 == formId)
		{
			listIterator = v6; //item->value
			if (listIterator != listMaxSize) break;
		}
		else
		{
			if (!ErectusProcess::Rpm(v7 + (v6 + v6 * 2) * 0x8 + 0x10, &v6, sizeof(v6))) return 0;
			if (v6 == listMaxSize) break;
		}
	}

	if (listIterator == listMaxSize) return 0;

	return v7 + (listIterator * 3) * 0x8 + 0x8;
}

std::uintptr_t ErectusMemory::GetPtr(const std::uint32_t formId)
{
	const auto address = GetAddress(formId);
	if (!address) return 0;

	std::uintptr_t ptr;
	if (!ErectusProcess::Rpm(address, &ptr, sizeof ptr)) return 0;
	return ptr;
}

bool ErectusMemory::IsRecipeKnown(const std::uint32_t formId)
{
	//the list of known recipes is implemented as a set / rb-tree.
	struct SetEntry
	{
		std::uintptr_t left; //0x0000
		std::uintptr_t  parent; //0x0008
		std::uintptr_t  right; //0x0010
		char pad0018; //0x0018
		BYTE isLeaf; //0x0019
		char pad001A[2]; //0x001A
		std::uint32_t value; //0x001C
	} setEntry = {};

	const auto player = Game::GetLocalPlayer();
	if (!player.IsIngame())
		return false;

	std::uintptr_t setPtr;
	if (!ErectusProcess::Rpm(player.playerKnownRecipes + 0x8, &setPtr, sizeof setPtr))
		return false;

	if (!ErectusProcess::Rpm(setPtr, &setEntry, sizeof setEntry))
		return false;

	while (!setEntry.isLeaf)
	{
		if (setEntry.value == formId)
			return true;
		if (setEntry.value > formId)
		{
			if (!ErectusProcess::Rpm(setEntry.left, &setEntry, sizeof setEntry))
				return false;
		}
		else
		{
			if (!ErectusProcess::Rpm(setEntry.right, &setEntry, sizeof setEntry))
				return false;
		}
	}
	return false;
}

bool ErectusMemory::CheckFormIdArray(const std::uint32_t formId, const bool* enabledArray, const std::uint32_t* formIdArray, const int size)
{
	for (auto i = 0; i < size; i++)
	{
		if (formId == formIdArray[i])
			return enabledArray[i];
	}
	return false;
}

bool ErectusMemory::CheckReferenceKeywordBook(const TesItem& referenceData, const std::uint32_t formId)
{
	if (!referenceData.keywordArrayData01C0 || referenceData.keywordArrayData01C0 > 0x7FFF)
		return false;
	if (!Utils::Valid(referenceData.keywordArrayData01B8))
		return false;

	const auto keywordArray = std::make_unique<std::uintptr_t[]>(referenceData.keywordArrayData01C0);
	if (!ErectusProcess::Rpm(referenceData.keywordArrayData01B8, keywordArray.get(), referenceData.keywordArrayData01C0 * sizeof(std::uintptr_t)))
		return false;

	for (std::uintptr_t i = 0; i < referenceData.keywordArrayData01C0; i++)
	{
		if (!Utils::Valid(keywordArray[i]))
			continue;

		std::uint32_t formIdCheck;
		if (!ErectusProcess::Rpm(keywordArray[i] + 0x20, &formIdCheck, sizeof formIdCheck))
			continue;
		if (formIdCheck != formId)
			continue;

		return true;
	}
	return false;
}

bool ErectusMemory::CheckReferenceKeywordMisc(const TesItem& referenceData, const std::uint32_t formId)
{
	if (!referenceData.keywordArrayData01B8 || referenceData.keywordArrayData01B8 > 0x7FFF)
		return false;
	if (!Utils::Valid(referenceData.keywordArrayData01B0))
		return false;

	const auto keywordArray = std::make_unique<std::uintptr_t[]>(referenceData.keywordArrayData01B8);
	if (!ErectusProcess::Rpm(referenceData.keywordArrayData01B0, keywordArray.get(), referenceData.keywordArrayData01B8 * sizeof(std::uintptr_t)))
		return false;

	for (size_t i = 0; i < referenceData.keywordArrayData01B8; i++)
	{
		if (!Utils::Valid(keywordArray[i]))
			continue;

		std::uint32_t formIdCheck;
		if (!ErectusProcess::Rpm(keywordArray[i] + 0x20, &formIdCheck, sizeof formIdCheck))
			continue;
		if (formIdCheck != formId)
			continue;

		return true;
	}
	return false;
}

bool ErectusMemory::CheckWhitelistedFlux(const TesItem& referenceData)
{
	if (!Utils::Valid(referenceData.harvestedPtr))
		return false;

	std::uint32_t formIdCheck;
	if (!ErectusProcess::Rpm(referenceData.harvestedPtr + 0x20, &formIdCheck, sizeof formIdCheck))
		return false;

	switch (formIdCheck)
	{
	case 0x002DDD45: //Raw Crimson Flux
		return Settings::esp.floraExt.crimsonFluxEnabled;
	case 0x002DDD46: //Raw Cobalt Flux
		return Settings::esp.floraExt.cobaltFluxEnabled;
	case 0x002DDD49: //Raw Yellowcake Flux
		return Settings::esp.floraExt.yellowcakeFluxEnabled;
	case 0x002DDD4B: //Raw Fluorescent Flux
		return Settings::esp.floraExt.fluorescentFluxEnabled;
	case 0x002DDD4D: //Raw Violet Flux
		return Settings::esp.floraExt.violetFluxEnabled;
	default:
		return false;
	}
}

bool ErectusMemory::IsItem(const TesItem& referenceData)
{
	switch (referenceData.GetFormType())
	{
	case FormType::TesUtilityItem:
	case FormType::BgsNote:
	case FormType::TesKey:
	case FormType::CurrencyObject:
		return true;
	default:
		return false;
	}
}

ItemInfo ErectusMemory::GetItemInfo(const TesObjectRefr& entity)
{
	ItemInfo result = {};

	result.refr = entity;
	result.base = entity.GetBaseObject();

	switch (result.base.GetFormType())
	{
	case FormType::BgsIdleMarker:
	case FormType::BgsStaticCollection:
	case FormType::TesObjectLigh:
	case FormType::TesObjectStat:
	case FormType::BgsMovableStatic:
	case FormType::TesSound:
	case FormType::BgsTextureSet:
	case FormType::BgsBendableSpline:
	case FormType::BgsAcousticSpace:
		result.type = ItemTypes::Invalid;
		break;

	case FormType::TesNpc:
		result.type = ItemTypes::Npc;
		break;

	case FormType::TesObjectCont:
		result.type = ItemTypes::Container;
		break;

	case FormType::TesObjectMisc:
		if (result.base.IsJunkItem())
			result.type = ItemTypes::Junk;
		else if (result.base.IsMod())
			result.type = ItemTypes::Mod;
		else if (result.base.IsBobblehead())
			result.type = ItemTypes::AidBobblehead;
		else
			result.type = ItemTypes::Misc;
		break;

	case FormType::TesObjectBook:
		if (result.base.IsPlan())
		{
			if (IsRecipeKnown(result.base.formId))
				result.type = ItemTypes::NotesKnownPlan;
			else
				result.type = ItemTypes::NotesUnknownPlan;
		}
		else if (result.base.IsMagazine())
			result.type = ItemTypes::AidMagazine;
		else if (result.base.IsTreasureMap())
			result.type = ItemTypes::NotesTreasureMap;
		else
			result.type = ItemTypes::Notes;
		break;

	case FormType::TesFlora:
		result.type = ItemTypes::Flora;
		break;

	case FormType::TesObjectWeap:
		result.type = ItemTypes::Weapons;
		break;

	case FormType::TesObjectArmo:
		result.type = ItemTypes::Apparel;
		break;

	case FormType::TesAmmo:
		result.type = ItemTypes::Ammo;
		break;

	case FormType::AlchemyItem:
		result.type = ItemTypes::Aid;
		break;

	case FormType::BgsNote:
		result.type = ItemTypes::Notes;
		break;

	case FormType::TesUtilityItem:
		result.type = ItemTypes::Aid;
		break;

	case FormType::TesKey:
		result.type = ItemTypes::Misc;
		break;

	case FormType::CurrencyObject:
		result.type = ItemTypes::Currency;
		break;

	default:
		result.type = ItemTypes::Other;
	}
	return result;
}

void ErectusMemory::GetCustomEntityData(const TesItem& baseObject, std::uintptr_t& entityFlag, int& enabledDistance)
{
	if (const auto found = Settings::esp.blacklist.find(baseObject.formId); found != Settings::esp.blacklist.end()) {
		if (found->second) {
			entityFlag |= CUSTOM_ENTRY_INVALID;
			return;
		}
	}

	if (const auto found = Settings::esp.whitelist.find(baseObject.formId); found != Settings::esp.whitelist.end()) {
		if (found->second)
			entityFlag |= CUSTOM_ENTRY_WHITELISTED;
	}

	switch (baseObject.GetFormType())
	{
	case FormType::BgsIdleMarker:
	case FormType::BgsStaticCollection:
	case FormType::TesObjectLigh:
	case FormType::TesObjectStat:
	case FormType::BgsMovableStatic:
	case FormType::TesSound:
	case FormType::BgsTextureSet:
	case FormType::BgsBendableSpline:
	case FormType::BgsAcousticSpace:
		entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	case FormType::TesNpc:
		entityFlag |= CUSTOM_ENTRY_NPC;
		enabledDistance = Settings::esp.npcs.enabledDistance;
		break;
	case FormType::TesObjectCont:
		entityFlag |= CUSTOM_ENTRY_CONTAINER;
		enabledDistance = Settings::esp.containers.enabledDistance;
		if (!Settings::esp.containers.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	case FormType::TesObjectMisc:
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		if (baseObject.IsJunkItem())
		{
			entityFlag |= CUSTOM_ENTRY_JUNK;
			enabledDistance = Settings::esp.junk.enabledDistance;
			if (!Settings::esp.junk.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else if (baseObject.IsMod())
		{
			entityFlag |= CUSTOM_ENTRY_MOD;
			entityFlag |= CUSTOM_ENTRY_ITEM;
			enabledDistance = Settings::esp.items.enabledDistance;
			if (!Settings::esp.items.enabled)
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else if (baseObject.IsBobblehead())
		{
			entityFlag |= CUSTOM_ENTRY_BOBBLEHEAD;
			enabledDistance = Settings::esp.bobbleheads.enabledDistance;
			if (!Settings::esp.bobbleheads.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else
		{
			entityFlag |= CUSTOM_ENTRY_MISC;
			entityFlag |= CUSTOM_ENTRY_ITEM;
			enabledDistance = Settings::esp.items.enabledDistance;
			if (!Settings::esp.items.enabled)
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		break;
	case FormType::TesObjectBook:
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		if (baseObject.IsPlan())
		{
			entityFlag |= CUSTOM_ENTRY_PLAN;
			enabledDistance = Settings::esp.plans.enabledDistance;

			if (IsRecipeKnown(baseObject.formId))
				entityFlag |= CUSTOM_ENTRY_KNOWN_RECIPE;
			else
				entityFlag |= CUSTOM_ENTRY_UNKNOWN_RECIPE;

			if (!Settings::esp.plans.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else if (baseObject.IsMagazine())
		{
			entityFlag |= CUSTOM_ENTRY_MAGAZINE;
			enabledDistance = Settings::esp.magazines.enabledDistance;
			if (!Settings::esp.magazines.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else
		{
			entityFlag |= CUSTOM_ENTRY_ITEM;
			enabledDistance = Settings::esp.items.enabledDistance;
			if (baseObject.IsTreasureMap())
				entityFlag |= CUSTOM_ENTRY_TREASURE_MAP;
			if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		break;
	case FormType::TesFlora:
		entityFlag |= CUSTOM_ENTRY_FLORA;
		enabledDistance = Settings::esp.flora.enabledDistance;

		if (CheckWhitelistedFlux(baseObject))
			entityFlag |= CUSTOM_ENTRY_WHITELISTED;
		if (!Settings::esp.flora.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;

		break;
	case FormType::TesObjectWeap:
		entityFlag |= CUSTOM_ENTRY_WEAPON;
		entityFlag |= CUSTOM_ENTRY_ITEM;
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		enabledDistance = Settings::esp.items.enabledDistance;
		if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	case FormType::TesObjectArmo:
		entityFlag |= CUSTOM_ENTRY_ARMOR;
		entityFlag |= CUSTOM_ENTRY_ITEM;
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		enabledDistance = Settings::esp.items.enabledDistance;
		if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	case FormType::TesAmmo:
		entityFlag |= CUSTOM_ENTRY_AMMO;
		entityFlag |= CUSTOM_ENTRY_ITEM;
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		enabledDistance = Settings::esp.items.enabledDistance;
		if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	case FormType::AlchemyItem:
		entityFlag |= CUSTOM_ENTRY_AID;
		entityFlag |= CUSTOM_ENTRY_ITEM;
		entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
		enabledDistance = Settings::esp.items.enabledDistance;
		if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
			entityFlag |= CUSTOM_ENTRY_INVALID;
		break;
	default:
		if (IsItem(baseObject))
		{
			entityFlag |= CUSTOM_ENTRY_ITEM;
			entityFlag |= CUSTOM_ENTRY_VALID_ITEM;
			enabledDistance = Settings::esp.items.enabledDistance;
			if (!Settings::esp.items.enabled && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		else
		{
			entityFlag |= CUSTOM_ENTRY_ENTITY;
			enabledDistance = Settings::esp.entities.enabledDistance;
			if ((!Settings::esp.entities.enabled || !Settings::esp.entities.drawUnnamed) && !(entityFlag & CUSTOM_ENTRY_WHITELISTED))
				entityFlag |= CUSTOM_ENTRY_INVALID;
		}
		break;
	}
}

std::string ErectusMemory::GetEntityName(const std::uintptr_t ptr)
{
	std::string result{};

	if (!Utils::Valid(ptr))
		return result;

	std::size_t nameLength = 0;
	auto namePtr = ptr;

	if (!ErectusProcess::Rpm(namePtr + 0x10, &nameLength, sizeof nameLength))
		return result;
	if (nameLength <= 0 || nameLength > 0x7FFF)
	{
		std::uintptr_t buffer;
		if (!ErectusProcess::Rpm(namePtr + 0x10, &buffer, sizeof buffer))
			return result;
		if (!Utils::Valid(buffer))
			return result;
		if (!ErectusProcess::Rpm(buffer + 0x10, &nameLength, sizeof nameLength))
			return result;
		if (nameLength <= 0 || nameLength > 0x7FFF)
			return result;
		namePtr = buffer;
	}

	const auto nameSize = nameLength + 1;
	const auto name = std::make_unique<char[]>(nameSize);

	if (!ErectusProcess::Rpm(namePtr + 0x18, name.get(), nameSize))
		return result;

	result = name.get();
	return result;
}

bool ErectusMemory::UpdateBufferEntityList()
{
	std::vector<CustomEntry> entities = {};

	const auto player = Game::GetLocalPlayer();

	auto cells = Game::GetLoadedCells();
	for (const auto& cell : cells)
	{
		auto references = cell.GetObjectRefs();
		entities.reserve(entities.size() + references.size());
		for (const auto& reference : references)
		{
			if (reference.GetFormType() == FormType::PlayerCharacter)
				continue;

			auto baseObject = reference.GetBaseObject();
			
			auto entityFlag = CUSTOM_ENTRY_DEFAULT;

			auto enabledDistance = 0;

			GetCustomEntityData(baseObject, entityFlag, enabledDistance);
			if (entityFlag & CUSTOM_ENTRY_INVALID)
				continue;

			if (reference.position.DistanceTo(player.position) * 0.01f > static_cast<float>(enabledDistance))
				continue;

			auto entityName = baseObject.GetName();
			if (entityName.empty())
			{
				entityFlag |= CUSTOM_ENTRY_UNNAMED;
				entityName = format(FMT_STRING("{0:X} [{1:X}]"), reference.formId, baseObject.formType);
			}

			entities.push_back({
				.entityPtr = reference.ptr,
				.baseObjectPtr = reference.baseObjectPtr,
				.entityFormId = reference.formId,
				.baseObjectFormId = baseObject.formId,
				.flag = entityFlag,
				.name = std::move(entityName)
				});
		}

	}
	entityDataBuffer = entities;

	return entityDataBuffer.empty() ? false : true;
}

std::string ErectusMemory::GetPlayerName(const ClientAccount& clientAccountData)
{
	std::string result = {};
	if (!clientAccountData.nameLength)
		return result;

	const auto playerNameSize = static_cast<std::size_t>(clientAccountData.nameLength) + 1;
	const auto playerName = std::make_unique<char[]>(playerNameSize);

	if (clientAccountData.nameLength > 15)
	{
		std::uintptr_t buffer;
		memcpy(&buffer, clientAccountData.nameData, sizeof buffer);
		if (!Utils::Valid(buffer))
			return result;

		if (!ErectusProcess::Rpm(buffer, playerName.get(), playerNameSize))
			return result;
	}
	else
		memcpy(playerName.get(), clientAccountData.nameData, playerNameSize);

	if (strlen(playerName.get()) != clientAccountData.nameLength)
		return result;

	result.assign(playerName.get());

	return result;
}

bool ErectusMemory::UpdateBufferPlayerList()
{
	std::vector<CustomEntry> players = {};

	const auto localPlayer = Game::GetLocalPlayer();
	if (!localPlayer.IsIngame())
		return false;

	std::uintptr_t falloutMainDataPtr;
	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_MAIN, &falloutMainDataPtr, sizeof falloutMainDataPtr))
		return false;
	if (!Utils::Valid(falloutMainDataPtr))
		return false;

	FalloutMain falloutMainData{};
	if (!ErectusProcess::Rpm(falloutMainDataPtr, &falloutMainData, sizeof falloutMainData))
		return false;
	if (!Utils::Valid(falloutMainData.platformSessionManagerPtr))
		return false;

	PlatformSessionManager platformSessionManagerData{};
	if (!ErectusProcess::Rpm(falloutMainData.platformSessionManagerPtr, &platformSessionManagerData, sizeof platformSessionManagerData))
		return false;
	if (!Utils::Valid(platformSessionManagerData.clientAccountManagerPtr))
		return false;

	ClientAccountManager clientAccountManagerData{};
	if (!ErectusProcess::Rpm(platformSessionManagerData.clientAccountManagerPtr, &clientAccountManagerData, sizeof clientAccountManagerData))
		return false;
	if (!Utils::Valid(clientAccountManagerData.clientAccountArrayPtr))
		return false;

	std::size_t clientAccountArraySize = 0;
	clientAccountArraySize += clientAccountManagerData.clientAccountArraySizeA;
	clientAccountArraySize += clientAccountManagerData.clientAccountArraySizeB;
	if (!clientAccountArraySize)
		return false;

	auto clientAccountArray = std::make_unique<std::uintptr_t[]>(clientAccountArraySize);
	if (!ErectusProcess::Rpm(clientAccountManagerData.clientAccountArrayPtr, clientAccountArray.get(), clientAccountArraySize * sizeof(std::uintptr_t)))
		return false;

	for (std::size_t i = 0; i < clientAccountArraySize; i++)
	{
		if (!Utils::Valid(clientAccountArray[i]))
			continue;

		ClientAccountBuffer clientAccountBufferData{};
		if (!ErectusProcess::Rpm(clientAccountArray[i], &clientAccountBufferData, sizeof clientAccountBufferData))
			continue;
		if (!Utils::Valid(clientAccountBufferData.clientAccountPtr))
			continue;

		ClientAccount clientAccountData{};
		if (!ErectusProcess::Rpm(clientAccountBufferData.clientAccountPtr, &clientAccountData, sizeof clientAccountData))
			continue;
		if (clientAccountData.formId == 0xFFFFFFFF)
			continue;

		if (clientAccountData.formId == localPlayer.formId)
			continue;

		auto entityPtr = GetPtr(clientAccountData.formId);
		if (!Utils::Valid(entityPtr))
			continue;

		TesObjectRefr reference{};
		if (!ErectusProcess::Rpm(entityPtr, &reference, sizeof reference))
			continue;

		auto baseObject = reference.GetBaseObject();
		
		if (baseObject.formId != 0x00000007)
			continue;

		auto entityFlag = CUSTOM_ENTRY_PLAYER;
		auto entityName = GetPlayerName(clientAccountData);
		if (entityName.empty())
		{
			entityFlag |= CUSTOM_ENTRY_UNNAMED;
			entityName = format(FMT_STRING("{:x}"), reference.formId);
		}

		CustomEntry entry{
			.entityPtr = entityPtr,
			.baseObjectPtr = reference.baseObjectPtr,
			.entityFormId = reference.formId,
			.baseObjectFormId = baseObject.formId,
			.flag = entityFlag,
			.name = std::move(entityName),
		};
		players.push_back(entry);
	}
	playerDataBuffer = players;

	return playerDataBuffer.empty() ? false : true;
}

bool ErectusMemory::IsTargetValid(const std::uintptr_t targetPtr)
{
	TesObjectRefr target{};
	if (!ErectusProcess::Rpm(targetPtr, &target, sizeof target))
		return false;

	return IsTargetValid(target);
}

bool ErectusMemory::IsTargetValid(const TesObjectRefr& targetData)
{
	if (targetData.GetFormType() != FormType::TesActor)
		return false;

	if (targetData.spawnFlag != 0x02)
		return false;

	if (Settings::targetting.ignoreEssentialNpcs && targetData.IsEssential())
		return false;

	if (Settings::targetting.ignoreNonHostileNpcs && !targetData.IsHostile())
		return false;

	switch (targetData.GetActorState())
	{
	case ActorState::Alive:
		return true;
	case ActorState::Downed:
	case ActorState::Dead:
		return false;
	case ActorState::Unknown:
		return Settings::targetting.targetUnknown;
	}
	return false;
}

bool ErectusMemory::IsFloraHarvested(const char harvestFlagA, const char harvestFlagB)
{
	return harvestFlagA >> 5 & 1 || harvestFlagB >> 5 & 1;
}

bool ErectusMemory::DamageRedirection(const std::uintptr_t targetPtr, std::uintptr_t& targetingPage, bool& targetingPageValid, const bool isExiting, const bool enabled)
{
	BYTE pageJmpOn[] = { 0x48, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE3 };
	BYTE pageJmpOff[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	BYTE pageJmpCheck[sizeof pageJmpOff];

	BYTE redirectionOn[] = { 0xE9, 0x8B, 0xFE, 0xFF, 0xFF }; //this should be calculated tbh, it's a relative jmp: "jmp (OFFSET_REDIRECTION_JMP - OFFSET_REDIRECTION)"
	BYTE redirectionOff[] = { 0x48, 0x8B, 0x5C, 0x24, 0x50 };
	BYTE redirectionCheck[sizeof redirectionOff];

	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_REDIRECTION_JMP, &pageJmpCheck, sizeof pageJmpCheck))
		return false;

	std::uintptr_t pageCheck;
	memcpy(&pageCheck, &pageJmpCheck[2], sizeof pageCheck);
	if (Utils::Valid(pageCheck) && pageCheck != targetingPage)
	{
		BYTE pageOpcode[] = { 0x48, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00 };
		BYTE pageOpcodeCheck[sizeof pageOpcode];
		if (!ErectusProcess::Rpm(pageCheck, &pageOpcodeCheck, sizeof pageOpcodeCheck))
			return false;
		if (memcmp(pageOpcodeCheck, pageOpcode, sizeof pageOpcode) != 0)
			return false;
		if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_REDIRECTION_JMP, &pageJmpOff, sizeof pageJmpOff))
			return false;
		if (!ErectusProcess::FreeEx(pageCheck))
			return false;
	}

	if (!targetingPage)
	{
		const auto page = ErectusProcess::AllocEx(sizeof(TargetLocking));
		if (!page)
			return false;
		targetingPage = page;
	}

	if (!targetingPageValid)
	{
		TargetLocking targetLockingData;
		targetLockingData.targetLockingPtr = targetPtr;
		auto originalFunction = ErectusProcess::exe + OFFSET_REDIRECTION + sizeof redirectionOff;
		std::uintptr_t originalFunctionCheck;
		if (!ErectusProcess::Rpm(targetingPage + 0x30, &originalFunctionCheck, sizeof originalFunctionCheck))
			return false;

		if (originalFunctionCheck != originalFunction)
			memcpy(&targetLockingData.redirectionAsm[0x30], &originalFunction, sizeof originalFunction);

		if (!ErectusProcess::Wpm(targetingPage, &targetLockingData, sizeof targetLockingData))
			return false;

		targetingPageValid = true;
		return false;
	}
	TargetLocking targetLockingData;
	if (!ErectusProcess::Rpm(targetingPage, &targetLockingData, sizeof targetLockingData))
		return false;

	if (targetLockingData.targetLockingPtr != targetPtr)
	{
		targetLockingData.targetLockingPtr = targetPtr;
		if (!ErectusProcess::Wpm(targetingPage, &targetLockingData, sizeof targetLockingData))
			return false;
		memcpy(&pageJmpOn[2], &targetingPage, sizeof(std::uintptr_t));
	}
	memcpy(&pageJmpOn[2], &targetingPage, sizeof(std::uintptr_t));

	const auto redirection = ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_REDIRECTION, &redirectionCheck,
		sizeof redirectionCheck);

	if (targetingPageValid && enabled && IsTargetValid(targetPtr))
	{
		if (redirection && !memcmp(redirectionCheck, redirectionOff, sizeof redirectionOff))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_REDIRECTION, &redirectionOn, sizeof redirectionOn);
	}
	else
	{
		if (redirection && !memcmp(redirectionCheck, redirectionOn, sizeof redirectionOn))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_REDIRECTION, &redirectionOff, sizeof redirectionOff);
	}

	if (targetingPageValid && !isExiting && !memcmp(pageJmpCheck, pageJmpOff, sizeof pageJmpOff))
		return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_REDIRECTION_JMP, &pageJmpOn, sizeof pageJmpOn);
	if (isExiting && !memcmp(pageJmpCheck, pageJmpOn, sizeof pageJmpOn))
		return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_REDIRECTION_JMP, &pageJmpOff, sizeof pageJmpOff);

	return true;
}

bool ErectusMemory::MovePlayer()
{
	std::uintptr_t bhkCharProxyControllerPtr;
	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_CHAR_CONTROLLER, &bhkCharProxyControllerPtr, sizeof bhkCharProxyControllerPtr))
		return false;
	if (!Utils::Valid(bhkCharProxyControllerPtr))
		return false;

	BhkCharProxyController bhkCharProxyControllerData{};
	if (!ErectusProcess::Rpm(bhkCharProxyControllerPtr, &bhkCharProxyControllerData, sizeof bhkCharProxyControllerData))
		return false;
	if (!Utils::Valid(bhkCharProxyControllerData.hknpBsCharacterProxyPtr))
		return false;

	HknpBsCharacterProxy hknpBsCharacterProxyData{};
	if (!ErectusProcess::Rpm(bhkCharProxyControllerData.hknpBsCharacterProxyPtr, &hknpBsCharacterProxyData, sizeof hknpBsCharacterProxyData))
		return false;


	float velocityA[4];
	memset(velocityA, 0x00, sizeof velocityA);

	float velocityB[4];
	memset(velocityB, 0x00, sizeof velocityB);

	auto speed = Settings::localPlayer.noclipSpeed;
	if (GetAsyncKeyState(VK_SHIFT))
		speed *= 1.5f;

	auto writePosition = false;
	const auto camera = Game::GetPlayerCamera();

	if (GetAsyncKeyState('W'))
	{
		hknpBsCharacterProxyData.position += camera.forward * speed;
		writePosition = true;
	}

	if (GetAsyncKeyState('S'))
	{
		hknpBsCharacterProxyData.position -= camera.forward * speed;
		writePosition = true;
	}

	if (GetAsyncKeyState('A'))
	{
		hknpBsCharacterProxyData.position.x -= camera.forward.y * speed;
		hknpBsCharacterProxyData.position.y += camera.forward.x * speed;
		writePosition = true;
	}


	if (GetAsyncKeyState('D'))
	{
		hknpBsCharacterProxyData.position.x += camera.forward.y * speed;
		hknpBsCharacterProxyData.position.y -= camera.forward.x * speed;
		writePosition = true;
	}

	if (memcmp(hknpBsCharacterProxyData.velocityA, velocityA, sizeof velocityA) != 0)
	{
		memcpy(hknpBsCharacterProxyData.velocityA, velocityA, sizeof velocityA);
		writePosition = true;
	}

	if (memcmp(hknpBsCharacterProxyData.velocityB, velocityB, sizeof velocityB) != 0)
	{
		memcpy(hknpBsCharacterProxyData.velocityB, velocityB, sizeof velocityB);
		writePosition = true;
	}

	if (writePosition)
		return ErectusProcess::Wpm(bhkCharProxyControllerData.hknpBsCharacterProxyPtr, &hknpBsCharacterProxyData, sizeof hknpBsCharacterProxyData);

	return true;
}

void ErectusMemory::Noclip(const bool enabled)
{
	BYTE noclipOnA[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
	BYTE noclipOffA[] = { 0xE8, 0xE3, 0xC1, 0xFE, 0xFF };
	BYTE noclipCheckA[sizeof noclipOffA];

	BYTE noclipOnB[] = { 0x0F, 0x1F, 0x40, 0x00 };
	BYTE noclipOffB[] = { 0x41, 0xFF, 0x50, 0x40 };
	BYTE noclipCheckB[sizeof noclipOffB];

	BYTE noclipOnC[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
	BYTE noclipOffC[] = { 0xE8, 0xEA, 0x59, 0x34, 0x01 };
	BYTE noclipCheckC[sizeof noclipOffC];

	BYTE noclipOnD[] = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
	BYTE noclipOffD[] = { 0xFF, 0x15, 0x49, 0x5B, 0xFF, 0x01 };
	BYTE noclipCheckD[sizeof noclipOffD];

	const auto noclipA = ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_NOCLIP_A, &noclipCheckA, sizeof noclipCheckA);
	const auto noclipB = ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_NOCLIP_B, &noclipCheckB, sizeof noclipCheckB);
	const auto noclipC = ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_NOCLIP_C, &noclipCheckC, sizeof noclipCheckC);
	const auto noclipD = ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_NOCLIP_D, &noclipCheckD, sizeof noclipCheckD);

	if (enabled)
	{
		if (noclipA && !memcmp(noclipCheckA, noclipOffA, sizeof noclipOffA))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_A, &noclipOnA, sizeof noclipOnA);

		if (noclipB && !memcmp(noclipCheckB, noclipOffB, sizeof noclipOffB))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_B, &noclipOnB, sizeof noclipOnB);

		if (noclipC && !memcmp(noclipCheckC, noclipOffC, sizeof noclipOffC))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_C, &noclipOnC, sizeof noclipOnC);

		if (noclipD && !memcmp(noclipCheckD, noclipOffD, sizeof noclipOffD))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_D, &noclipOnD, sizeof noclipOnD);

		MovePlayer();
	}
	else
	{
		if (noclipA && !memcmp(noclipCheckA, noclipOnA, sizeof noclipOnA))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_A, &noclipOffA, sizeof noclipOffA);

		if (noclipB && !memcmp(noclipCheckB, noclipOnB, sizeof noclipOnB))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_B, &noclipOffB, sizeof noclipOffB);

		if (noclipC && !memcmp(noclipCheckC, noclipOnC, sizeof noclipOnC))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_C, &noclipOffC, sizeof noclipOffC);

		if (noclipD && !memcmp(noclipCheckD, noclipOnD, sizeof noclipOnD))
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_NOCLIP_D, &noclipOffD, sizeof noclipOffD);
	}
}


bool ErectusMemory::ReferenceSwap(std::uint32_t& sourceFormId, std::uint32_t& destinationFormId)
{
	if (sourceFormId == destinationFormId)
		return true;

	auto sourcePointer = GetPtr(sourceFormId);
	if (!Utils::Valid(sourcePointer))
	{
		sourceFormId = 0x00000000;
		return false;
	}

	const auto destinationAddress = GetAddress(destinationFormId);
	if (!Utils::Valid(destinationAddress))
	{
		destinationFormId = 0x00000000;
		return false;
	}
	return ErectusProcess::Wpm(destinationAddress, &sourcePointer, sizeof sourcePointer);
}

bool ErectusMemory::CheckItemTransferList()
{
	for (auto i = 0; i < 32; i++)
	{
		if (Settings::customTransferSettings.whitelist[i] && Settings::customTransferSettings.whitelisted[i])
			return true;
	}
	return false;
}

bool ErectusMemory::TransferItems(const std::uint32_t sourceFormId, const std::uint32_t destinationFormId)
{
	auto sourcePtr = GetPtr(sourceFormId);
	if (!Utils::Valid(sourcePtr))
		return false;

	auto destinationPtr = GetPtr(destinationFormId);
	if (!Utils::Valid(destinationPtr))
		return false;

	TesObjectRefr entityData{};
	if (!ErectusProcess::Rpm(sourcePtr, &entityData, sizeof entityData))
		return false;

	auto inventory = entityData.GetInventory();
	for (const auto& inventoryEntry : inventory) {
		if (!Utils::Valid(inventoryEntry.baseObjectPtr))
			continue;
		if (!Utils::Valid(inventoryEntry.displayPtr) || inventoryEntry.iterations <= inventoryEntry.displayPtr)
			continue;

		if (Settings::customTransferSettings.useWhitelist || Settings::customTransferSettings.useBlacklist)
		{
			TesItem referenceData{};
			if (!ErectusProcess::Rpm(inventoryEntry.baseObjectPtr, &referenceData, sizeof referenceData))
				continue;

			if (Settings::customTransferSettings.useWhitelist)
			{
				if (!CheckFormIdArray(referenceData.formId, Settings::customTransferSettings.whitelisted, Settings::customTransferSettings.whitelist, 32))
					continue;
			}

			if (Settings::customTransferSettings.useBlacklist)
			{
				if (CheckFormIdArray(referenceData.formId, Settings::customTransferSettings.blacklisted, Settings::customTransferSettings.blacklist, 32))
					continue;
			}
		}

		auto iterations = (inventoryEntry.iterations - inventoryEntry.displayPtr) / sizeof(ItemCount);
		auto itemCountData = std::make_unique<ItemCount[]>(iterations);
		if (!ErectusProcess::Rpm(inventoryEntry.displayPtr, itemCountData.get(), iterations * sizeof(ItemCount)))
			continue;

		auto count = 0;
		for (std::uintptr_t c = 0; c < iterations; c++)
		{
			count += itemCountData[c].count;
		}
		if (count == 0)
			continue;

		TransferMessage transferMessageData = {
			.vtable = ErectusProcess::exe + VTABLE_REQUESTTRANSFERITEMMSG,
			.sourceEntityId = sourceFormId,
			.playerEntityId = 0x0,
			.bShouldSendResult = true,
			.destEntityId = destinationFormId,
			.itemServerHandleId = inventoryEntry.itemId,
			.stashAccessEntityId = 0x0,
			.bCreateIfMissing = false,
			.bIsExpectingResult = false,
			.count = count,
		};
		MsgSender::Send(&transferMessageData, sizeof transferMessageData);
	}
	return true;
}

bool ErectusMemory::SaveTeleportPosition(const int index)
{
	const auto player = Game::GetLocalPlayer();
	if (!player.IsIngame())
		return false;

	Settings::teleporter.entries[index].position = player.position;
	Settings::teleporter.entries[index].rotation.z = player.yaw;
	Settings::teleporter.entries[index].cellFormId = player.GetCurrentCell().formId;

	return true;
}

bool ErectusMemory::RequestTeleport(const int index)
{
	//const auto cellPtr = GetPtr(Settings::teleporter.entries[index].cellFormId);
	//if (!Utils::Valid(cellPtr))
	//	return false;

	RequestTeleportMessage requestTeleportMessageData =
	{
		.vtable = ErectusProcess::exe + VTABLE_REQUESTTELEPORTTOLOCATIONMSG,
		.position = Settings::teleporter.entries[index].position,
		.rotation = Settings::teleporter.entries[index].rotation,
		.cellPtr = 0,//cellPtr,
		.unk = 1
	};
	return MsgSender::Send(&requestTeleportMessageData, sizeof requestTeleportMessageData);
}

bool ErectusMemory::FreezeActionPoints(std::uintptr_t& freezeApPage, bool& freezeApPageValid, const bool enabled)
{
	if (!freezeApPage && !Settings::localPlayer.freezeApEnabled)
		return false;

	if (!freezeApPage)
	{
		const auto page = ErectusProcess::AllocEx(sizeof(FreezeAp));
		if (!page)
			return false;

		freezeApPage = page;
	}

	BYTE freezeApOn[]
	{
		0x0F, 0x1F, 0x40, 0x00, //nop [rax+00]
		0x48, 0xBF, //mov rdi (Page)
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Page (mov rdi)
		0xFF, 0xE7, //jmp rdi
		0x0F, 0x1F, 0x40, 0x00, //nop [rax+00]
	};

	BYTE freezeApOff[]
	{
		0x8B, 0xD6, //mov edx, esi
		0x48, 0x8B, 0xC8, //mov rcx, rax
		0x48, 0x8B, 0x5C, 0x24, 0x30, //mov rbx, [rsp+30]
		0x48, 0x8B, 0x74, 0x24, 0x38, //mov rsi, [rsp+38]
		0x48, 0x83, 0xC4, 0x20, //add rsp, 20
		0x5F, //pop rdi
	};

	BYTE freezeApCheck[sizeof freezeApOff];

	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_AV_REGEN, &freezeApCheck, sizeof freezeApCheck))
		return false;

	std::uintptr_t pageCheck;
	memcpy(&pageCheck, &freezeApCheck[0x6], sizeof(std::uintptr_t));

	if (Utils::Valid(pageCheck) && pageCheck != freezeApPage)
	{
		for (auto i = 0; i < 0x6; i++) if (freezeApCheck[i] != freezeApOn[i])
			return false;
		if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_AV_REGEN, &freezeApOff, sizeof freezeApOff))
			return false;
		ErectusProcess::FreeEx(pageCheck);
	}

	if (enabled)
	{
		FreezeAp freezeApData;
		freezeApData.freezeApEnabled = Settings::localPlayer.freezeApEnabled;

		if (freezeApPageValid)
		{
			FreezeAp freezeApPageCheck;
			if (!ErectusProcess::Rpm(freezeApPage, &freezeApPageCheck, sizeof freezeApPageCheck))
				return false;
			if (!memcmp(&freezeApData, &freezeApPageCheck, sizeof freezeApPageCheck))
				return true;
			return ErectusProcess::Wpm(freezeApPage, &freezeApData, sizeof freezeApData);
		}
		if (!ErectusProcess::Wpm(freezeApPage, &freezeApData, sizeof freezeApData))
			return false;
		memcpy(&freezeApOn[0x6], &freezeApPage, sizeof(std::uintptr_t));
		if (!ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_AV_REGEN, &freezeApOn, sizeof freezeApOn))
			return false;
		freezeApPageValid = true;
	}
	else
	{
		if (pageCheck == freezeApPage)
			ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_AV_REGEN, &freezeApOff, sizeof freezeApOff);

		if (freezeApPage)
		{
			if (ErectusProcess::FreeEx(freezeApPage))
			{
				freezeApPage = 0;
				freezeApPageValid = false;
			}
		}
	}
	return true;
}

bool ErectusMemory::SetClientState(const std::uintptr_t clientState)
{
	const auto player = Game::GetLocalPlayer();
	if (!player.IsIngame() || player.GetCurrentCell().isInterior)
		return false;

	ClientStateMsg clientStateMsgData{ .vtable = ErectusProcess::exe + VTABLE_CLIENTSTATEMSG, .clientState = clientState };

	return MsgSender::Send(&clientStateMsgData, sizeof clientStateMsgData);
}

bool ErectusMemory::PositionSpoofing(const bool enabled)
{
	BYTE positionSpoofingOn[] = { 0xBA, 0x01, 0x00, 0xF8, 0xFF, 0xEB, 0x1D };
	BYTE positionSpoofingOff[] = { 0xBA, 0x01, 0x00, 0xF8, 0xFF, 0xF3, 0x0F };

	BYTE positionSpoofingCheck[sizeof positionSpoofingOff];

	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_SERVER_POSITION, &positionSpoofingCheck, sizeof positionSpoofingCheck))
		return false;

	if (!enabled)
	{
		positionSpoofingCheck[1] = 0x00;
		positionSpoofingCheck[2] = 0x00;
		positionSpoofingCheck[3] = 0x00;
		positionSpoofingCheck[4] = 0x00;

		if (!memcmp(positionSpoofingCheck, positionSpoofingOn, sizeof positionSpoofingOn))
			return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_SERVER_POSITION, &positionSpoofingOff, sizeof positionSpoofingOff);

		return true;
	}

	int spoofingHeightCheck;
	memcpy(&spoofingHeightCheck, &positionSpoofingCheck[1], sizeof spoofingHeightCheck);
	memcpy(&positionSpoofingOn[1], &Settings::localPlayer.positionSpoofingHeight,
		sizeof Settings::localPlayer.positionSpoofingHeight);

	if (!memcmp(positionSpoofingCheck, positionSpoofingOn, sizeof positionSpoofingOn))
		return true;
	if (!memcmp(positionSpoofingCheck, positionSpoofingOff, sizeof positionSpoofingOff))
		return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_SERVER_POSITION, &positionSpoofingOn, sizeof positionSpoofingOn);
	if (spoofingHeightCheck != Settings::localPlayer.positionSpoofingHeight)
	{
		if (positionSpoofingCheck[0] != 0xBA || spoofingHeightCheck < -524287 || spoofingHeightCheck > 524287)
			return false;

		return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_SERVER_POSITION, &positionSpoofingOn, sizeof positionSpoofingOn);
	}
	return false;
}

std::uint32_t ErectusMemory::GetEntityId(const TesObjectRefr& entityData)
{
	if (!(entityData.idValue[0] & 1)) return 0;

	uint32_t v1;
	memcpy(&v1, entityData.idValue, sizeof(v1));

	uint32_t v2 = v1 >> 1;
	uint32_t v3 = v2 + v2;
	
	uintptr_t id = 0;
	if (!ErectusProcess::Rpm(ErectusProcess::exe + OFFSET_ENTITY_ID , &id, sizeof(id))) return 0;

	uint32_t v4 = 0;
	if (!ErectusProcess::Rpm(id + v3 * 0x8, &v4, sizeof(v4))) return 0;

	uint32_t v5 = v4 & 0x7FF80000;
	uint32_t v6 = v5 | v2;

	return v6;
}

bool ErectusMemory::SendHitsToServer(Hits* hitsData, const size_t hitsDataSize)
{
	const auto allocSize = sizeof(ExternalFunction) + sizeof(RequestHitsOnActors) + hitsDataSize;
	const auto allocAddress = ErectusProcess::AllocEx(allocSize);
	if (allocAddress == 0)
		return false;

	ExternalFunction externalFunctionData;
	externalFunctionData.address = ErectusProcess::exe + OFFSET_MESSAGE_SENDER;
	externalFunctionData.rcx = allocAddress + sizeof(ExternalFunction);
	externalFunctionData.rdx = 0;
	externalFunctionData.r8 = 0;
	externalFunctionData.r9 = 0;

	const auto pageData = std::make_unique<BYTE[]>(allocSize);
	memset(pageData.get(), 0x00, allocSize);

	RequestHitsOnActors requestHitsOnActorsData{};
	memset(&requestHitsOnActorsData, 0x00, sizeof(RequestHitsOnActors));

	requestHitsOnActorsData.vtable = ErectusProcess::exe + VTABLE_REQUESTHITSONACTORS;
	requestHitsOnActorsData.hitsArrayPtr = allocAddress + sizeof(ExternalFunction) + sizeof(RequestHitsOnActors);
	requestHitsOnActorsData.hitsArrayEnd = allocAddress + sizeof(ExternalFunction) + sizeof(RequestHitsOnActors) + hitsDataSize;

	memcpy(pageData.get(), &externalFunctionData, sizeof externalFunctionData);
	memcpy(&pageData[sizeof(ExternalFunction)], &requestHitsOnActorsData, sizeof requestHitsOnActorsData);
	memcpy(&pageData[sizeof(ExternalFunction) + sizeof(RequestHitsOnActors)], &*hitsData, hitsDataSize);

	const auto pageWritten = ErectusProcess::Wpm(allocAddress, pageData.get(), allocSize);

	if (!pageWritten)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto paramAddress = allocAddress + sizeof ExternalFunction::ASM;
	auto* const thread = CreateRemoteThread(ErectusProcess::handle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(allocAddress),
		reinterpret_cast<LPVOID>(paramAddress), 0, nullptr);

	if (thread == nullptr)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto threadResult = WaitForSingleObject(thread, 3000);
	CloseHandle(thread);

	if (threadResult == WAIT_TIMEOUT)
		return false;

	ErectusProcess::FreeEx(allocAddress);

	return true;
}

bool ErectusMemory::SendDamage(const std::uintptr_t targetPtr, const std::uint32_t weaponId, BYTE* shotsHit, BYTE* shotsFired, const BYTE count)
{
	if (!Settings::targetting.dmgSend)
		return false;

	if (!weaponId)
		return false;

	if (!MsgSender::IsEnabled())
		return false;

	const auto player = Game::GetLocalPlayer();
	if (!player.IsIngame())
		return false;

	TesObjectRefr target{};
	if (!ErectusProcess::Rpm(targetPtr, &target, sizeof target))
		return false;

	if (target.GetFormType() == FormType::PlayerCharacter)
		return false;

	if (!IsTargetValid(target))
		return false;

	const auto localPlayerId = GetEntityId(player);
	if (!localPlayerId)
		return false;

	const auto targetId = GetEntityId(target);
	if (!targetId)
		return false;

	const auto hitsData = std::make_unique<Hits[]>(count);
	memset(hitsData.get(), 0x00, count * sizeof(Hits));

	if (*shotsHit == 0 || *shotsHit == 255)
		*shotsHit = 1;

	if (*shotsFired == 255)
		*shotsFired = 0;

	for (BYTE i = 0; i < count; i++)
	{
		//hitsData[i].valueB = localPlayerId;
		//hitsData[i].valueA = targetId;
		//hitsData[i].valueC = 0;
		//hitsData[i].initializationType = 0x3;
		//hitsData[i].uiWeaponServerId = weaponId;
		//hitsData[i].limbEnum = 0xFFFFFFFF;
		//hitsData[i].hitEffectId = 0;
		//hitsData[i].uEquipIndex = 0;
		//hitsData[i].uAckIndex = *shotsHit;
		//hitsData[i].uFireId = *shotsFired;
		//hitsData[i].bPredictedKill = 0;
		//hitsData[i].padding0023 = 0;
		//hitsData[i].explosionLocationX = 0.0f;
		//hitsData[i].explosionLocationY = 0.0f;
		//hitsData[i].explosionLocationZ = 0.0f;
		//hitsData[i].fProjectilePower = 1.0f;
		//hitsData[i].bVatsAttack = 0;
		//hitsData[i].bVatsCritical = 0;
		//hitsData[i].bTargetWasDead = 0;
		//hitsData[i].padding0037 = 0;

		hitsData[i].target = targetId;
		hitsData[i].source = localPlayerId;
		hitsData[i].initializationType = 0x3;
		hitsData[i].uiWeaponServerId = weaponId;
		hitsData[i].limbEnum = 0xFFFFFFFF;
		hitsData[i].uAckIndex = *shotsHit;
		hitsData[i].uFireId = *shotsFired;
		hitsData[i].fProjectilePower = 1.0f;


		if (Settings::targetting.sendDamageMax < 10)
		{
			if (Utils::GetRangedInt(1, 10) <= 10 - Settings::targetting.sendDamageMax)
			{
				if (*shotsHit == 0 || *shotsHit == 255)
					*shotsHit = 1;
				else
					*shotsHit += 1;
			}
			else
			{
				*shotsHit = 1;
			}
		}
		else
			*shotsHit = 1;

		for (auto c = 0; c < Utils::GetRangedInt(1, 6); c++)
		{
			if (*shotsFired == 255)
				*shotsFired = 0;
			else
				*shotsFired += 1;
		}
	}

	const auto result = SendHitsToServer(hitsData.get(), count * sizeof(Hits));

	return result;
}

std::uint32_t ErectusMemory::GetFavoritedWeaponId(const BYTE favouriteIndex)
{
	if (Settings::targetting.favoriteIndex >= 12)
		return 0;

	auto inventory = Game::GetLocalPlayer().GetInventory();
	for (const auto& inventoryEntry : inventory)
	{
		if (!Utils::Valid(inventoryEntry.baseObjectPtr))
			continue;
		if (inventoryEntry.favoriteIndex != favouriteIndex)
			continue;

		TesItem referenceData{};
		if (!ErectusProcess::Rpm(inventoryEntry.baseObjectPtr, &referenceData, sizeof referenceData))
			break;
		if (referenceData.GetFormType() != FormType::TesObjectWeap)
			break;

		return inventoryEntry.itemId;
	}
	return 0;
}

char ErectusMemory::FavoriteIndex2Slot(const BYTE favoriteIndex)
{
	switch (favoriteIndex)
	{
	case 0x00:
		return '1';
	case 0x01:
		return '2';
	case 0x02:
		return '3';
	case 0x03:
		return '4';
	case 0x04:
		return '5';
	case 0x05:
		return '6';
	case 0x06:
		return '7';
	case 0x07:
		return '8';
	case 0x08:
		return '9';
	case 0x09:
		return '0';
	case 0x0A:
		return '-';
	case 0x0B:
		return '=';
	default:
		return '?';
	}
}

std::uintptr_t ErectusMemory::RttiGetNamePtr(const std::uintptr_t vtable)
{
	std::uintptr_t buffer;
	if (!ErectusProcess::Rpm(vtable - 0x8, &buffer, sizeof buffer))
		return 0;
	if (!Utils::Valid(buffer))
		return 0;

	std::uint32_t offset;
	if (!ErectusProcess::Rpm(buffer + 0xC, &offset, sizeof offset))
		return 0;
	if (offset == 0 || offset > 0x7FFFFFFF)
		return 0;

	return ErectusProcess::exe + offset + 0x10;
}

std::string ErectusMemory::GetInstancedItemName(const std::uintptr_t displayPtr)
{
	std::string result{};

	if (!Utils::Valid(displayPtr))
		return result;

	std::uintptr_t instancedArrayPtr;
	if (!ErectusProcess::Rpm(displayPtr, &instancedArrayPtr, sizeof instancedArrayPtr))
		return result;
	if (!Utils::Valid(instancedArrayPtr))
		return result;

	ItemInstancedArray itemInstancedArrayData{};
	if (!ErectusProcess::Rpm(instancedArrayPtr, &itemInstancedArrayData, sizeof itemInstancedArrayData))
		return result;
	if (!Utils::Valid(itemInstancedArrayData.arrayPtr) || itemInstancedArrayData.arrayEnd <= itemInstancedArrayData.arrayPtr)
		return result;

	const auto instancedArraySize = (itemInstancedArrayData.arrayEnd - itemInstancedArrayData.arrayPtr) / sizeof(std::uintptr_t);

	const auto instancedArray = std::make_unique<std::uintptr_t[]>(instancedArraySize);
	if (!ErectusProcess::Rpm(itemInstancedArrayData.arrayPtr, instancedArray.get(), instancedArraySize * sizeof(std::uintptr_t)))
		return result;

	for (std::uintptr_t i = 0; i < instancedArraySize; i++)
	{
		if (!Utils::Valid(instancedArray[i]))
			continue;

		ExtraTextDisplayData extraTextDisplayDataData{};
		if (!ErectusProcess::Rpm(instancedArray[i], &extraTextDisplayDataData, sizeof extraTextDisplayDataData))
			continue;

		const auto rttiNamePtr = RttiGetNamePtr(extraTextDisplayDataData.vtable);
		if (!rttiNamePtr)
			continue;

		char rttiNameCheck[sizeof".?AVExtraTextDisplayData@@"];
		if (!ErectusProcess::Rpm(rttiNamePtr, &rttiNameCheck, sizeof rttiNameCheck))
			continue;
		if (strcmp(rttiNameCheck, ".?AVExtraTextDisplayData@@") != 0)
			continue;

		result = GetEntityName(extraTextDisplayDataData.instancedNamePtr);
		break;
	}
	return result;
}

std::unordered_map<int, std::string> ErectusMemory::GetFavoritedWeapons()
{
	std::unordered_map<int, std::string> result = {
		{0, "[?] No Weapon Selected"},
		{1, "[1] Favorited Item Invalid"},
		{2, "[2] Favorited Item Invalid"},
		{3, "[3] Favorited Item Invalid"},
		{4, "[4] Favorited Item Invalid"},
		{5, "[5] Favorited Item Invalid"},
		{6, "[6] Favorited Item Invalid"},
		{7, "[7] Favorited Item Invalid"},
		{8, "[8] Favorited Item Invalid"},
		{9, "[9] Favorited Item Invalid"},
		{10, "[0] Favorited Item Invalid"},
		{11, "[-] Favorited Item Invalid"},
		{12, "[=] Favorited Item Invalid"},
		{13, "[?] Favorited Item Invalid"},
	};

	auto inventory = Game::GetLocalPlayer().GetInventory();

	for (const auto& inventoryEntry : inventory)
	{
		if (!Utils::Valid(inventoryEntry.baseObjectPtr))
			continue;
		if (inventoryEntry.favoriteIndex > 12)
			continue;

		TesItem referenceData{};
		if (!ErectusProcess::Rpm(inventoryEntry.baseObjectPtr, &referenceData, sizeof referenceData))
			continue;
		if (referenceData.GetFormType() != FormType::TesObjectWeap)
			continue;

		auto weaponName = GetInstancedItemName(inventoryEntry.displayPtr);
		if (weaponName.empty())
		{
			weaponName = referenceData.GetName();
			if (weaponName.empty())
				continue;
		}

		result[inventoryEntry.favoriteIndex + 1] = format(FMT_STRING("[{}] {}"), FavoriteIndex2Slot(inventoryEntry.favoriteIndex), weaponName);
	}
	return result;
}

bool ErectusMemory::MeleeAttack()
{
	if (!MsgSender::IsEnabled())
		return false;

	const auto player = Game::GetLocalPlayer();
	if (!player.IsIngame())
		return false;

	const auto allocAddress = ErectusProcess::AllocEx(sizeof(ExternalFunction));
	if (allocAddress == 0)
		return false;

	ExternalFunction externalFunctionData = {
		.address = ErectusProcess::exe + OFFSET_MELEE_ATTACK,
		.rcx = player.ptr,
		.rdx = 0,
		.r8 = 1,
		.r9 = 0,
	};

	const auto written = ErectusProcess::Wpm(allocAddress, &externalFunctionData, sizeof(ExternalFunction));

	if (!written)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto paramAddress = allocAddress + sizeof ExternalFunction::ASM;
	auto* const thread = CreateRemoteThread(ErectusProcess::handle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(allocAddress),
		reinterpret_cast<LPVOID>(paramAddress), 0, nullptr);

	if (thread == nullptr)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}

	const auto threadResult = WaitForSingleObject(thread, 3000);
	CloseHandle(thread);

	if (threadResult == WAIT_TIMEOUT)
		return false;

	ErectusProcess::FreeEx(allocAddress);
	return true;
}

bool ErectusMemory::VtableSwap(const std::uintptr_t dst, std::uintptr_t src)
{
	DWORD oldProtect;
	if (!VirtualProtectEx(ErectusProcess::handle, reinterpret_cast<void*>(dst), sizeof(std::uintptr_t), PAGE_READWRITE, &oldProtect))
		return false;

	const auto result = ErectusProcess::Wpm(dst, &src, sizeof src);

	DWORD buffer;
	if (!VirtualProtectEx(ErectusProcess::handle, reinterpret_cast<void*>(dst), sizeof(std::uintptr_t), oldProtect, &buffer))
		return false;

	return result;
}

bool ErectusMemory::PatchIntegrityCheck()
{
	char check = 0;
	return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_INTEGRITYCHECK, &check, sizeof check);
}

bool ErectusMemory::PatchDetectFlag()
{
	BYTE patch[] = { 0x31, 0xC0, 0x90};
	return ErectusProcess::Wpm(ErectusProcess::exe + OFFSET_FLAGDETECTED, &patch, sizeof patch);
}