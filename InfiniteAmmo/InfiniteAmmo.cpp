#include "Global.h"

UInt16 uNeverEndingCapacity;
UInt32 uMinAmmoCapacityMult;
bool bUseInfiniteAmmo;
bool bUseInfiniteThrowableWeapon;
bool bUseWhiteListMode;
std::unordered_set<UInt32> excludedWeapons;
std::unordered_set<UInt32> includedWeapons;

bool IsExcludedWeapon(UInt32 weapFormId) {
	return excludedWeapons.find(weapFormId) != excludedWeapons.end();
}

bool IsIncludedWeapon(UInt32 weapFormId) {
	return includedWeapons.find(weapFormId) != includedWeapons.end();
}

UInt16 GetAmmoType(TESObjectWEAP::InstanceData* weapInst, UInt16 ammoCapacity) {
	UInt16 ammoType = AmmoType::kAmmoType_Default;

	// ���� ����� ������ �ִ� ���� ���� ź�෮�� 0�϶�: ������ 
	if (ammoCapacity == 0)
		ammoType |= AmmoType::kAmmoType_NeverEnding;

	UInt32 ammoHealth = reinterpret_cast<UInt32&>(weapInst->ammo->unk160[1]);
	// ���� ����� ������ ź���� Health�� 0�� �ƴҶ�: ǻ���ھ�
	if (ammoHealth != 0)
		ammoType |= AmmoType::kAmmoType_FusionCore;

	// ���� ����� ������ �÷��׿� ChargingReload�� ������: ������ ����
	if (weapInst->flags & TESObjectWEAP::InstanceData::WeaponFlags::kFlag_ChargingReload)
		ammoType |= AmmoType::kAmmoType_Charging;

	return ammoType;
}

void AddAmmo(TESForm* weapForm, TESObjectWEAP::InstanceData* weapInst) {
	if (!weapForm || !weapInst)
		return;

	// ź�� ���� �ɼ��� �������� �� ����
	if (!bUseInfiniteAmmo)
		return;

	// ���� ���Ⱑ ���� ������ ��� ����
	if (IsExcludedWeapon(weapForm->formID))
		return;

	// ȭ��Ʈ����Ʈ ��尡 �����ְ� ���� ���Ⱑ ȭ��Ʈ����Ʈ�� ���Ե��� �ʴ� ��� ����
	if (bUseWhiteListMode && !IsIncludedWeapon(weapForm->formID))
		return;

	// ź���� �������� �ʴ� ������ ��� ����
	TESForm* ammo = weapInst->ammo;
	if (!ammo)
		return;

	UInt16 ammoCapacity = CurrentAmmoCapacity;

	// ������ ������ ��� ������ ���� �⺻ źȯ ������ �̿�
	if (GetAmmoType(weapInst, ammoCapacity) & AmmoType::kAmmoType_NeverEnding)
		ammoCapacity = uNeverEndingCapacity;

	// ���� źȯ�� �� �������� ��ȸ
	UInt32 totalAmmoCount = GetInventoryItemCount(*g_player, ammo);

	// ź�� �߰�
	if (totalAmmoCount < ammoCapacity * uMinAmmoCapacityMult) {
		UInt32 diff = ammoCapacity * uMinAmmoCapacityMult - totalAmmoCount;
		AddItem(*g_player, ammo, diff, true);
	}
}

bool IsInfiniteThrowable(TESForm* weapForm) {
	if (!weapForm)
		return false;

	// ��ô���� ������ �ƴ� ��� ����
	if (!bUseInfiniteThrowableWeapon)
		return false;

	// ���ܹ��⿡ ���Ե� ��� ����
	if (IsExcludedWeapon(weapForm->formID))
		return false;

	// ȭ��Ʈ����Ʈ ����̰� ȭ��Ʈ ����Ʈ ���Ⱑ �ƴ� ��� ����
	if (bUseWhiteListMode && !IsIncludedWeapon(weapForm->formID))
		return false;

	return true;
}

bool IsNeverEndingWeapon(TESForm* weapForm, TESObjectWEAP::InstanceData* weapInst) {
	if (!weapInst || !weapInst->ammo)
		return false;

	// ź�๫���� �ƴ� ��� ����
	if (!bUseInfiniteAmmo)
		return false;

	// ���ܹ��⿡ ���Ե� ��� ����
	if (IsExcludedWeapon(weapForm->formID))
		return false;

	// ȭ��Ʈ����Ʈ ����̰� ȭ��Ʈ ����Ʈ ���Ⱑ �ƴ� ��� ����
	if (bUseWhiteListMode && !IsIncludedWeapon(weapForm->formID))
		return false;

	return GetAmmoType(weapInst, CurrentAmmoCapacity) & AmmoType::kAmmoType_NeverEnding;
}

char GetNextChar(const std::string& line, UInt32& index) {
	if (index < line.length())
		return line[index++];

	return -1;
}

std::string GetNextData(const std::string& line, UInt32& index, char delimeter) {
	char ch;
	std::string retVal = "";

	while ((ch = GetNextChar(line, index)) > 0) {
		if (ch == '#') {
			if (index > 0) index--;
			break;
		}

		if (delimeter != 0 && ch == delimeter)
			break;

		retVal += ch;
	}

	trim(retVal);
	return retVal;
}

void LoadInfiniteAmmoSetting() {
	std::string settingFilePath{ "Data\\F4SE\\Plugins\\"  PLUGIN_NAME  ".txt" };
	std::fstream settingFile(settingFilePath);
	if (!settingFile.is_open()) {
		_MESSAGE("Can't open plugin setting file!");
		return;
	}

	uNeverEndingCapacity = 10;
	uMinAmmoCapacityMult = 2;
	bUseInfiniteAmmo = true;
	bUseInfiniteThrowableWeapon = true;
	bUseWhiteListMode = false;

	excludedWeapons.clear();
	includedWeapons.clear();

	std::string line;
	std::string lineType, optionName, optionValueStr, pluginName, formId;
	while (std::getline(settingFile, line)) {
		trim(line);
		if (line.length() == 0 || line[0] == '#')
			continue;

		UInt32 index = 0;

		lineType = GetNextData(line, index, '|');
		if (lineType.length() == 0) {
			_MESSAGE("Can't read Line Type[%s]", line.c_str());
			continue;
		}

		if (lineType == "Option") {
			optionName = GetNextData(line, index, '=');
			if (optionName.length() == 0) {
				_MESSAGE("Can't read Option Name[%s]", line.c_str());
				continue;
			}

			optionValueStr = GetNextData(line, index, 0);
			if (optionValueStr.length() == 0) {
				_MESSAGE("Can't read Option Value[%s]", line.c_str());
				continue;
			}

			std::stringstream optionValue(optionValueStr);
			if (optionValue.fail()) {
				_MESSAGE("Can't read Option Value[%s]", line.c_str());
				continue;
			}

			if (optionName == "uNeverEndingCapacity")
				optionValue >> uNeverEndingCapacity;
			else if (optionName == "uMinAmmoCapacityMult")
				optionValue >> uMinAmmoCapacityMult;
			else if (optionName == "bUseInfiniteAmmo")
				optionValue >> bUseInfiniteAmmo;
			else if (optionName == "bUseInfiniteThrowableWeapon")
				optionValue >> bUseInfiniteThrowableWeapon;
			else if (optionName == "bUseWhiteListMode")
				optionValue >> bUseWhiteListMode;
			else {
				_MESSAGE("Unknown Option Name[%s]", line.c_str());
				continue;
			}
		}
		else if (lineType == "Exclude" || lineType == "Include") {
			pluginName = GetNextData(line, index, '|');
			if (pluginName.length() == 0) {
				_MESSAGE("Can't read Plugin Name[%s]", line.c_str());
				continue;
			}

			formId = GetNextData(line, index, 0);
			if (formId.length() == 0) {
				_MESSAGE("Can't read Form ID[%s]", line.c_str());
				continue;
			}

			TESForm* weapForm = GetFormFromIdentifier(pluginName, formId);
			if (!weapForm) {
				_MESSAGE("Can't find Weapon[%s]", line.c_str());
				continue;
			}

			if (lineType == "Exclude")
				excludedWeapons.insert(weapForm->formID);
			else if (lineType == "Include")
				includedWeapons.insert(weapForm->formID);

			_MESSAGE("%s Weapon: %s | 0x%08X", lineType, pluginName, weapForm->formID);
		}
		else {
			_MESSAGE("Can't determine Line Type[%s]", line.c_str());
			continue;
		}
	}

	settingFile.close();

	_MESSAGE("uNeverEndingCapacity: %u", uNeverEndingCapacity);
	_MESSAGE("uMinAmmoCapacityMult: %u", uMinAmmoCapacityMult);
	_MESSAGE("bUseInfiniteAmmo: %u", bUseInfiniteAmmo);
	_MESSAGE("bUseInfiniteThrowableWeapon: %u", bUseInfiniteThrowableWeapon);
	_MESSAGE("bUseWhiteListMode: %u", bUseWhiteListMode);
}