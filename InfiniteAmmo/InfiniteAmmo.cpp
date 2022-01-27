#include "Global.h"

UInt32 uNeverEndingCapacity;
UInt32 uMinAmmoCapacityMult;
bool bUseInfiniteAmmo;
bool bUseInfiniteThrowableWeapon;
bool bUseWhiteListMode;
std::unordered_set<UInt32> excludedWeapons;
std::unordered_set<UInt32> includedWeapons;

bool IsExcludedWeapon(UInt32 weapFormId) {
	if (excludedWeapons.find(weapFormId) != excludedWeapons.end())
		return true;
	return false;
}

bool IsIncludedWeapon(UInt32 weapFormId) {
	if (includedWeapons.find(weapFormId) != includedWeapons.end())
		return true;
	return false;
}

void CheckAmmo(TESForm* weapForm, TESObjectWEAP::InstanceData* weapInst, UInt32 shotCount, bool forcedReplenishAmmo) {
	// ź�� ���� �ɼ��� �������� �� ����
	if (!bUseInfiniteAmmo)
		return;

	// ���� ���Ⱑ ���� ������ ��� ����
	if (!weapForm || IsExcludedWeapon(weapForm->formID))
		return;

	// ȭ��Ʈ����Ʈ ��尡 �����ְ� ���� ���Ⱑ ȭ��Ʈ����Ʈ�� ���Ե��� �ʴ� ��� ����
	if (bUseWhiteListMode && !IsIncludedWeapon(weapForm->formID))
		return;

	if (!weapInst) {
		TESObjectWEAP* objWeap = DYNAMIC_CAST(weapForm, TESForm, TESObjectWEAP);
		if (!objWeap)
			return;
		weapInst = &objWeap->weapData;
	}

	// ź���� �������� �ʴ� ������ ��� ����
	TESForm* ammo = weapInst->ammo;
	if (!ammo)	// Melee Weapons
		return;

	// ź���� Ÿ���� üũ
	UInt16 ammoType = AmmoType::kAmmoType_Default;

	// ���� ����� ������ �ִ� ���� ���� ź�෮�� 0�϶�: ������ 
	if (CurrentAmmoCapacity == 0)
		ammoType |= AmmoType::kAmmoType_NeverEnding;

	UInt32 ammoHealth = static_cast<UInt32>(weapInst->ammo->unk160[1]);
	// ���� ����� ������ ź���� Health�� 0�� �ƴҶ�: ǻ���ھ�
	if (ammoHealth != 0)
		ammoType |= AmmoType::kAmmoType_FusionCore;

	// ���� ����� ������ �÷��׿� ChargingReload�� ������: ������ ����
	if (weapInst->flags & TESObjectWEAP::InstanceData::WeaponFlags::kFlag_ChargingReload)
		ammoType |= AmmoType::kAmmoType_Changing;

	// ���� �����Ǿ��ִ� ź���� ���� ����
	UInt32 loadedAmmoCount = 0;
	Actor::MiddleProcess::Data08::EquipData* equipData = GetEquipDataByFormID(weapForm->formID);
	if (equipData && equipData->equippedData)
		loadedAmmoCount = static_cast<UInt32>(equipData->equippedData->unk18);

	// ������ ������ ��� ���� ������ źȯ�� ���� �Ҹ��
	if (shotCount > 0 && ammoType & AmmoType::kAmmoType_Changing)
		shotCount = loadedAmmoCount;

	// �߻�� źȯ���� ����Ͽ� ���������� �����Ǿ��ִ� źȯ�� ���� �����
	UInt32 calculatedLoadedAmmoCount = loadedAmmoCount > shotCount ? loadedAmmoCount - shotCount : 0;

	// ź�� Ÿ���� ������ & (������ ���� | ǻ���ھ�)�� ��� üũ ����
	if (ammoType & AmmoType::kAmmoType_NeverEnding && ammoType & (AmmoType::kAmmoType_Changing | AmmoType::kAmmoType_FusionCore))
		return;

	// ������ ������ ź�� ä�� �ʿ䰡 ���� ���
	if (!forcedReplenishAmmo) {
		// ������ ������ �ƴϰ� ���� ������ źȯ���� 0�� �ƴ� ��� üũ ����
		if (ammoType ^ AmmoType::kAmmoType_NeverEnding && calculatedLoadedAmmoCount != 0) {
			return;
		}
		// ������ �����϶� ���� ������ źȯ���� [������ ���� �ּ� ź�� * (�ּ�ź�� ��� - 1)] ���� Ŭ�� üũ ����
		else if (ammoType & AmmoType::kAmmoType_NeverEnding) {
			UInt32 minNeverEndingAmmoCheckLimit = uMinAmmoCapacityMult > 1 ? uNeverEndingCapacity * (uMinAmmoCapacityMult - 1) : 0;
			if (calculatedLoadedAmmoCount > minNeverEndingAmmoCheckLimit)
				return;
		}
	}

	// ���� źȯ�� �� �������� ��ȸ
	UInt32 totalAmmoCount = GetInventoryItemCount(*g_player, ammo);

	// ���� ������ ���������� źȯ ���� ��ȸ
	UInt32 ammoCapacity = 0;
	// �Ϲ� ������ ��� ������ ���������� źȯ ������ �̿��ϸ� ��
	if (ammoType == AmmoType::kAmmoType_Default || ammoType == AmmoType::kAmmoType_Changing)
		ammoCapacity = CurrentAmmoCapacity;
	// ������ ������ ��� ������ ���� �⺻ źȯ ������ �̿�
	else if (ammoType & AmmoType::kAmmoType_NeverEnding)
		ammoCapacity = uNeverEndingCapacity;
	// ǻ���ھ� ������ ��� ������ ǻ���ھ� 1��
	else if (ammoType & AmmoType::kAmmoType_FusionCore)
		ammoCapacity = 1;

	// 7. ������ �߰�
	UInt32 calculatedTotalAmmoCount = totalAmmoCount > shotCount ? totalAmmoCount - shotCount : 0;
	if (calculatedTotalAmmoCount < ammoCapacity * uMinAmmoCapacityMult) {
		UInt32 diff = ammoCapacity * uMinAmmoCapacityMult - calculatedTotalAmmoCount;
		AddItem(*g_player, ammo, diff, true);
	}
}

bool IsInfiniteThrowable(TESForm* weapForm) {
	if (!weapForm)
		return false;

	// ��ô���� ������ �ƴ� ��� ����
	if (!bUseInfiniteThrowableWeapon)
		return false;

	// ���� ���⿡ ���Ե� ��� ����
	if (IsExcludedWeapon(weapForm->formID))
		return false;

	// ȭ��Ʈ����Ʈ ����̰� ȭ��Ʈ ����Ʈ ���Ⱑ �ƴ� ��� ����
	if (bUseWhiteListMode && !IsIncludedWeapon(weapForm->formID))
		return false;

	return true;
}

std::string GetNextData(const std::string& line, UInt32& index, char delimeter) {
	std::string retVal = "";
	for (; index < line.length(); index++) {
		if (delimeter != 0 && line[index] == delimeter) {
			index++;
			break;
		}

		retVal += line[index];
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