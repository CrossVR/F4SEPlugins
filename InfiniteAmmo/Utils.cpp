#include "Global.h"

RelocAddr <_AddItem> AddItem_Internal(0x4279D0);

void AddItem(TESObjectREFR* refr, TESForm* item, UInt32 count, bool isSilent) {
	if (!refr || !item)
		return;

	if (count == 0)
		return;

	AddItemData addItemData = { refr, 0, isSilent };
	ItemData itemData = { item, 0, count, 0, 0, 0, 1.0 };

	AddItem_Internal(&addItemData, &itemData);
}

Actor::MiddleProcess::Data08::EquipData* GetEquipDataByFormID(UInt32 formId) {
	if (!*g_player || !(*g_player)->middleProcess || !(*g_player)->middleProcess->unk08)
		return nullptr;;

	tArray<Actor::MiddleProcess::Data08::EquipData> equipDataArray = reinterpret_cast<tArray<Actor::MiddleProcess::Data08::EquipData> &>((*g_player)->middleProcess->unk08->equipData);
	if (equipDataArray.count == 0)
		return nullptr;

	for (UInt32 ii = 0; ii < equipDataArray.count; ii++) {
		if (equipDataArray.entries[ii].item->formID == formId)
			return &equipDataArray.entries[ii];
	}

	return nullptr;
}

UInt32 GetEquipIndex(Actor::MiddleProcess::Data08::EquipData* equipData) {
	UInt32 index = static_cast<UInt32>(equipData->unk18);
	return index;
}

static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
}

static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

template <typename T>
T GetOffset(const void* baseObject, int offset) {
	return *reinterpret_cast<T*>((uintptr_t)baseObject + offset);
}

TESForm* GetFormFromIdentifier(const std::string& identifier) {
	auto delimiter = identifier.find('|');
	if (delimiter != std::string::npos) {
		std::string modName = identifier.substr(0, delimiter);
		std::string modForm = identifier.substr(delimiter + 1);

		const ModInfo* mod = (*g_dataHandler)->LookupModByName(modName.c_str());
		if (mod && mod->modIndex != -1) {
			UInt32 formID = std::stoul(modForm, nullptr, 16) & 0xFFFFFF;
			UInt32 flags = GetOffset<UInt32>(mod, 0x334);
			if (flags & (1 << 9)) {
				// ESL
				formID &= 0xFFF;
				formID |= 0xFE << 24;
				formID |= GetOffset<UInt16>(mod, 0x372) << 12;	// ESL load order
			}
			else {
				formID |= (mod->modIndex) << 24;
			}
			return LookupFormByID(formID);
		}
	}
	return nullptr;
}