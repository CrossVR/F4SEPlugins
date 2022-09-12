#include "Global.h"

std::unordered_map<UInt32, ActorData> actorMap;
std::unordered_map<UInt64, SceneData> sceneMap;

UInt64 sceneMapKey = 1;
UInt32 selectedActorFormId = 0;
bool positionerEnabled = false;

enum CanMove {
	kCanMove_YES = 0,
	kCanMove_NO_SELECTION,
	kCanMove_NO_SCALE,
	kCanMove_NO_DISABLED
};

namespace Positioner {
	ActorData* GetActorDataByFormId(UInt32 formId) {
		auto actorIt = actorMap.find(formId);
		if (actorIt == actorMap.end())
			return nullptr;

		return &actorIt->second;
	}

	ActorData* GetPlayerActorData() {
		if (!*g_player)
			return nullptr;

		return GetActorDataByFormId((*g_player)->formID);
	}

	ActorData* GetSelectedActorData() {
		if (!selectedActorFormId)
			return nullptr;

		return GetActorDataByFormId(selectedActorFormId);
	}

	SceneData* GetSceneDataById(UInt64 sceneId) {
		auto sceneIt = sceneMap.find(sceneId);
		if (sceneIt == sceneMap.end())
			return nullptr;

		return &sceneIt->second;
	}

	UInt64 GetSceneIdFromActorList(VMArray<Actor*>& actorList) {
		for (UInt32 ii = 0; ii < actorList.Length(); ii++) {
			Actor* actorPtr = nullptr;
			actorList.Get(&actorPtr, ii);

			ActorData* actorData = GetActorDataByFormId(actorPtr->formID);
			if (!actorData)
				continue;

			// ���͸� �̿��Ͽ� ���� ID�� ������
			return actorData->sceneId;
		}

		return 0;
	}

	bool IsSceneHasPlayer(SceneData* sceneData) {
		if (!sceneData)
			return false;

		ActorData* playerActorData = GetPlayerActorData();
		if (!playerActorData)
			return false;

		for (UInt32 formId : sceneData->actorList)
			if (formId == playerActorData->formId)
				return true;

		return false;
	}

	bool IsActorInPlayerScene(ActorData* actorData) {
		if (!actorData)
			return false;

		ActorData* playerActorData = GetPlayerActorData();
		if (!playerActorData)
			return false;

		if (actorData->sceneId == playerActorData->sceneId)
			return true;

		return false;
	}

	bool IsActorScale1(Actor* actor) {
		if (!actor)
			return false;

		float actorScale = Utility::GetActualScale(actor);
		UInt32 intScale = static_cast<UInt32>(std::round(actorScale * 100));
		if (intScale == 100)
			return true;
		return false;
	}

	std::vector<PositionData::Data> LoadPosition(SceneData* sceneData) {
		if (!sceneData)
			return std::vector<PositionData::Data>();

		if (g_pluginSettings.bSeparatePlayerOffset)
			return PositionData::LoadPositionData(sceneData->position, IsSceneHasPlayer(sceneData));
		else
			return PositionData::LoadPositionData(sceneData->position, false);
	}

	void SavePosition(SceneData* sceneData) {
		if (!sceneData)
			return;

		if (g_pluginSettings.bSeparatePlayerOffset)
			PositionData::SavePositionData(sceneData->position, sceneData->actorList, IsSceneHasPlayer(sceneData));
		else
			PositionData::SavePositionData(sceneData->position, sceneData->actorList, false);
	}

	void SavePosition(ActorData* actorData) {
		if (!actorData)
			return;

		SavePosition(GetSceneDataById(actorData->sceneId));
	}

	bool GetPositionData(PositionData::Data& o_data, const std::vector<PositionData::Data> posVec, UInt32 index) {
		for (PositionData::Data data : posVec)
			if (data.index == index) {
				o_data = data;
				return true;
			}

		return false;
	}

	void GetOffsetFromPositionData(NiPoint3& offset, const std::vector<PositionData::Data> posVec, UInt32 posIdx) {
		PositionData::Data data;
		if (GetPositionData(data, posVec, posIdx))
			offset = data.offset;
		else
			offset = NiPoint3();
	}

	NiPoint3 GetRotatedOffset(const NiPoint3& offset, float rot) {
		float rotatedOffsetX = offset.x * cos(rot) + offset.y * sin(rot);
		float rotatedOffsetY = -offset.x * sin(rot) + offset.y * cos(rot);
		float rotatedOffsetZ = offset.z;
		return NiPoint3(rotatedOffsetX, rotatedOffsetY, rotatedOffsetZ);
	}

	void ApplyOffset(ActorData* actorData) {
		PositionerType positionerType = IsActorInPlayerScene(actorData) ? g_pluginSettings.iPlayerPositionerType : g_pluginSettings.iNPCPositionerType;
		if (positionerType == PositionerType::kPositionerType_Relative && IsActorScale1(actorData->actor))
			return;

		NiPoint3 rotatedOffset = GetRotatedOffset(actorData->offset, actorData->actor->rot.z);

		if (actorData->extraRefPath) {
			if (positionerType == PositionerType::kPositionerType_Relative) {
				float scale = Utility::GetActualScale(actorData->actor);
				float scaleDiff = 1.0f - scale;
				actorData->extraRefPath->goalPos.x = actorData->originPos.x + rotatedOffset.x * scaleDiff;
				actorData->extraRefPath->goalPos.y = actorData->originPos.y + rotatedOffset.y * scaleDiff;
				actorData->extraRefPath->goalPos.z = actorData->originPos.z + rotatedOffset.z * scaleDiff;
			}
			else if (positionerType == PositionerType::kPositionerType_Absolute) {
				actorData->extraRefPath->goalPos.x = actorData->originPos.x + rotatedOffset.x;
				actorData->extraRefPath->goalPos.y = actorData->originPos.y + rotatedOffset.y;
				actorData->extraRefPath->goalPos.z = actorData->originPos.z + rotatedOffset.z;
			}

			Utility::ModPos(actorData->actor, 'X', actorData->extraRefPath->goalPos.x);
			Utility::ModPos(actorData->actor, 'Y', actorData->extraRefPath->goalPos.y);
			Utility::ModPos(actorData->actor, 'Z', actorData->extraRefPath->goalPos.z);
		}
	}

	ExtraRefrPath* GetExtraRefrPath(Actor* actor) {
		if (!actor)
			return nullptr;

		BSExtraData* extraData = actor->extraDataList->m_data;
		while (extraData) {
			ExtraRefrPath* refrPath = DYNAMIC_CAST(extraData, BSExtraData, ExtraRefrPath);
			if (refrPath)
				return refrPath;

			extraData = extraData->next;
		}

		return nullptr;
	}

	void SetOffset(const std::string& axis, float offset) {
		if (!positionerEnabled)
			return;

		ActorData* actorData = GetSelectedActorData();
		if (!actorData)
			return;

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->actor);
		if (!extraRefPath)
			return;

		if (actorData->extraRefPath != extraRefPath) {
			actorData->extraRefPath = extraRefPath;
			actorData->originPos = extraRefPath->goalPos;
		}

		if (axis == "X")
			actorData->offset.x = offset;
		else if (axis == "Y")
			actorData->offset.y = offset;
		else if (axis == "Z")
			actorData->offset.z = offset;

		ApplyOffset(actorData);
		SavePosition(actorData);
	}

	bool IsPositionerEnabled(StaticFunctionTag*) {
		return positionerEnabled;
	}

	void SetPositionerState(StaticFunctionTag*, bool start) {
		if (start) {
			positionerEnabled = true;
		}
		else {
			positionerEnabled = false;
			selectedActorFormId = 0;
		}
	}

	void SceneInit(StaticFunctionTag*, VMArray<Actor*> actors, Actor* doppelganger) {
		SceneData newScene;

		// �� �� �ʱ�ȭ
		newScene.sceneId = sceneMapKey++;

		for (UInt32 ii = 0; ii < actors.Length(); ii++) {
			Actor* actorPtr = nullptr;
			actors.Get(&actorPtr, ii);

			bool isPlayerActor = false;

			// ���� ���Ͱ� �÷��̾��� ��� ������ ���ð�� ���
			if (actorPtr == *g_player) {
				isPlayerActor = true;
				actorPtr = doppelganger;
				if (!actorPtr)
					continue;

				if (g_pluginSettings.bUnifyAAFDoppelgangerScale)
					Utility::SetScale(actorPtr, Utility::GetActualScale(*g_player));
			}

			// ���� ���� �ʱ�ȭ
			ActorData actorData;
			actorData.formId = isPlayerActor ? (*g_player)->formID : actorPtr->formID;
			actorData.actor = actorPtr;
			actorData.sceneId = newScene.sceneId;
			actorData.extraRefPath = nullptr;
			actorData.offset = NiPoint3();
			actorData.originPos = NiPoint3();

			// �ʱ�ȭ�� ���� ������ ���� ���� ����Ʈ�� ����
			newScene.actorList.push_back(actorData.formId);

			// ���� ���� ����Ʈ�� ���Ե� ���� ������ �����͸� ���� �ʿ� �߰���
			actorMap.insert(std::make_pair(actorData.formId, actorData));
		}

		// ���� �� �ʿ� ����
		sceneMap.insert(std::make_pair(newScene.sceneId, newScene));
	}

	void AnimationChange(StaticFunctionTag*, BSFixedString position, VMArray<Actor*> actors) {
		UInt64 sceneId = GetSceneIdFromActorList(actors);
		if (!sceneId)
			return;

		// �� ID�� �̿��Ͽ� �� �ʿ��� ���� ã�´�
		SceneData* sceneData = GetSceneDataById(sceneId);
		if (!sceneData)
			return;

		sceneData->position = position;

		// �� ������ �̿��Ͽ� ��ġ ������ �о��
		std::vector<PositionData::Data> posDataVec = LoadPosition(sceneData);

		for (UInt32 ii = 0; ii < actors.Length(); ii++) {
			Actor* actorPtr = nullptr;
			actors.Get(&actorPtr, ii);

			ActorData* actorData = GetActorDataByFormId(actorPtr->formID);
			if (!actorData)
				continue;

			actorData->posIndex = ii;
			GetOffsetFromPositionData(actorData->offset, posDataVec, ii);

			if (Scaleform::PositionerMenu::IsMenuOpen() && actorData->formId == selectedActorFormId)
				Scaleform::PositionerMenu::UpdateMenu(actorData->offset.x, actorData->offset.y, actorData->offset.z);

			// ������ ���� ��ġ�� �����ϴ� ExtraRefrPath�� �ҷ���
			ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->actor);
			if (!actorData->extraRefPath && !extraRefPath)
				continue;

			// ExtraRefPath�� ������ ���� ���
			if (actorData->extraRefPath && actorData->extraRefPath == extraRefPath) {
				// ��ġ ���� �� ��ǥ�� ����
				actorData->extraRefPath->goalPos = actorData->originPos;
			}
			// ExtraRefPath�� ���� ���
			else {
				actorData->extraRefPath = extraRefPath;
				if (extraRefPath)
					actorData->originPos = extraRefPath->goalPos;
			}

			// ���� ������ ����
			ApplyOffset(actorData);
		}
	}

	void SceneEnd(StaticFunctionTag*, VMArray<Actor*> actors) {
		UInt64 sceneId = GetSceneIdFromActorList(actors);
		if (!sceneId)
			return;

		for (UInt32 ii = 0; ii < actors.Length(); ii++) {
			Actor* actorPtr = nullptr;
			actors.Get(&actorPtr, ii);

			ActorData* actorData = GetActorDataByFormId(actorPtr->formID);
			if (!actorData)
				continue;

			if (Scaleform::PositionerMenu::IsMenuOpen() && actorData->formId == selectedActorFormId)
				Scaleform::PositionerMenu::CloseMenu();

			// ������ ���Ͱ� ����Ǵ� ���� ���ԵǾ����� ��� ������ ���͸� �ʱ�ȭ
			if (selectedActorFormId == actorData->formId)
				selectedActorFormId = 0;

			actorMap.erase(actorData->formId);
		}

		sceneMap.erase(sceneId);
	}

	void MoveAxis(StaticFunctionTag*, BSFixedString axis, bool isInc) {
		if (!positionerEnabled)
			return;

		ActorData* actorData = GetSelectedActorData();
		if (!actorData)
			return;

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->actor);
		if (!extraRefPath)
			return;

		if (actorData->extraRefPath != extraRefPath) {
			actorData->extraRefPath = extraRefPath;
			actorData->originPos = extraRefPath->goalPos;
		}

		if (isInc) {
			if (axis == BSFixedString("X"))
				actorData->offset.x += g_pluginSettings.fMoveAxisSize;
			else if (axis == BSFixedString("Y"))
				actorData->offset.y += g_pluginSettings.fMoveAxisSize;
			else if (axis == BSFixedString("Z"))
				actorData->offset.z += g_pluginSettings.fMoveAxisSize;
		}
		else {
			if (axis == BSFixedString("X"))
				actorData->offset.x -= g_pluginSettings.fMoveAxisSize;
			else if (axis == BSFixedString("Y"))
				actorData->offset.y -= g_pluginSettings.fMoveAxisSize;
			else if (axis == BSFixedString("Z"))
				actorData->offset.z -= g_pluginSettings.fMoveAxisSize;
		}

		ApplyOffset(actorData);
		SavePosition(actorData);
	}

	UInt32 CanMovePosition(StaticFunctionTag*) {
		if (!positionerEnabled)
			return CanMove::kCanMove_NO_DISABLED;

		ActorData* selectedActorData = GetSelectedActorData();
		if (!selectedActorData)
			return CanMove::kCanMove_NO_SELECTION;

		PositionerType positionerType = IsActorInPlayerScene(selectedActorData) ? g_pluginSettings.iPlayerPositionerType : g_pluginSettings.iNPCPositionerType;

		// ��ġ ���� Ÿ���� �������̰� ���� �������� 1�̸� �̵� �Ұ�
		if (positionerType == PositionerType::kPositionerType_Relative && IsActorScale1(selectedActorData->actor))
			return CanMove::kCanMove_NO_SCALE;

		return CanMove::kCanMove_YES;
	}

	Actor* ChangeSelectedActor(StaticFunctionTag*) {
		if (!positionerEnabled)
			return nullptr;

		ActorData* actorData;
		SceneData* sceneData;

		// ���� ���õǾ��ִ� ���͸� ������
		actorData = GetSelectedActorData();
		// ���� ���õǾ��ִ� ���Ͱ� ���� ���
		if (!actorData) {
			// �÷��̾�� �������� ���� �ִ��� Ȯ��
			actorData = GetPlayerActorData();
			if (actorData) {
				// �÷��̾�� �������� ���� ���� �� �� ������ ������
				SceneData* playerScene = GetSceneDataById(actorData->sceneId);
				if (!playerScene || playerScene->actorList.size() == 0)
					return nullptr;

				// �ش� ���� ���� ù ���͸� �����Ͽ� ��ȯ
				actorData = GetActorDataByFormId(playerScene->actorList.front());
				if (!actorData)
					return nullptr;

				selectedActorFormId = actorData->formId;
				return actorData->actor;
			}

			// �÷��̾�� �������� ���� ���� ���
			// ù ���� ù ���͸� �����Ͽ� ��ȯ��
			if (sceneMap.size() == 0)
				return nullptr;

			sceneData = &sceneMap.begin()->second;
			if (sceneData->actorList.size() == 0)
				return nullptr;

			actorData = GetActorDataByFormId(sceneData->actorList.front());
			if (!actorData)
				return nullptr;

			selectedActorFormId = actorData->formId;
			return actorData->actor;
		}

		// ���� ���õǾ��ִ� ���Ͱ� ���� ��� ���� ���Ͱ� ���Ե� ���� ������
		auto sceneIt = sceneMap.find(actorData->sceneId);
		if (sceneIt == sceneMap.end())
			return nullptr;

		sceneData = &sceneIt->second;

		// ���� ���õ� ���͸� ã�� �� ������ ���� ���͸� ã�Ƽ� ��ȯ
		auto actorIt = std::find(sceneData->actorList.begin(), sceneData->actorList.end(), actorData->formId);
		if (actorIt == sceneData->actorList.end())
			return nullptr;

		actorIt = std::next(actorIt);
		if (actorIt != sceneData->actorList.end()) {
			actorData = GetActorDataByFormId(*actorIt);
			if (!actorData)
				return nullptr;

			selectedActorFormId = actorData->formId;
			return actorData->actor;
		}

		// ���õ� ���Ͱ� �ش� ���� ������ ���Ϳ��� ���
		// �ش� ���� ���� ���� ù ���͸� ã�Ƽ� ��ȯ
		sceneIt = std::next(sceneIt);
		if (sceneIt != sceneMap.end()) {
			sceneData = &sceneIt->second;
			if (sceneData->actorList.size() == 0)
				return nullptr;

			actorData = GetActorDataByFormId(sceneData->actorList.front());
			if (!actorData)
				return nullptr;

			selectedActorFormId = actorData->formId;
			return actorData->actor;
		}

		// ���õ� ���Ͱ� ������ ���� ������ ���Ϳ��� ���
		// ���� ù ���� ù ���͸� ã�Ƽ� ��ȯ
		sceneData = &sceneMap.begin()->second;
		if (sceneData->actorList.size() == 0)
			return nullptr;

		actorData = GetActorDataByFormId(sceneData->actorList.front());
		if (!actorData)
			return nullptr;

		selectedActorFormId = actorData->formId;
		return actorData->actor;
	}

	Actor* GetSelectedActor(StaticFunctionTag*) {
		ActorData* selectedActorData = GetSelectedActorData();
		if (!selectedActorData)
			return nullptr;

		return selectedActorData->actor;
	}

	SpellItem* GetHighlightSpell(StaticFunctionTag*, bool isMovable) {
		static TESForm* movableSpellForm = Utility::GetFormFromIdentifier("AAFDynamicPositioner.esp", 0x00000810);
		static TESForm* immovableSpellForm = Utility::GetFormFromIdentifier("AAFDynamicPositioner.esp", 0x00000811);
		if (!movableSpellForm || !immovableSpellForm)
			return nullptr;

		SpellItem* spell = DYNAMIC_CAST(isMovable ? movableSpellForm : immovableSpellForm, TESForm, SpellItem);
		return spell;
	}

	void ClearSelectedActorOffset(StaticFunctionTag*) {
		if (!positionerEnabled)
			return;

		ActorData* selectedActorData = GetSelectedActorData();
		if (!selectedActorData)
			return;

		selectedActorData->offset = NiPoint3();

		ApplyOffset(selectedActorData);
		SavePosition(selectedActorData);
	}

	void ShowSelectedSceneOffset(StaticFunctionTag*) {
		if (!positionerEnabled)
			return;

		ActorData* selectedActorData = GetSelectedActorData();
		if (!selectedActorData)
			return;

		SceneData* selectedSceneData = GetSceneDataById(selectedActorData->sceneId);
		if (!selectedSceneData)
			return;

		std::string msg = selectedSceneData->position + "\n";
		for (UInt32 ii = 0; ii < selectedSceneData->actorList.size(); ii++) {
			for (UInt32& actorId : selectedSceneData->actorList) {
				ActorData* pActorData = GetActorDataByFormId(actorId);
				if (!pActorData)
					continue;

				if (ii == pActorData->posIndex) {
					if (pActorData == selectedActorData)
						msg += "[*]";
					else
						msg += "[" + std::to_string(ii) + "]";
					msg += std::to_string(pActorData->offset.x) + ", " + std::to_string(pActorData->offset.y) + ", " + std::to_string(pActorData->offset.z) + "\n";
				}
			}
		}
		Utility::ShowMessagebox(msg);
	}

	void ShowPositionerMenuNative(StaticFunctionTag*) {
		if (!positionerEnabled)
			return;

		ActorData* actorData = GetSelectedActorData();
		if (!actorData)
			return;

		ExtraRefrPath* extraRefPath = GetExtraRefrPath(actorData->actor);
		if (!extraRefPath)
			return;

		if (actorData->extraRefPath != extraRefPath) {
			actorData->extraRefPath = extraRefPath;
			actorData->originPos = extraRefPath->goalPos;
		}

		Scaleform::PositionerMenu::OpenMenu(actorData->offset.x, actorData->offset.y, actorData->offset.z);
	}

	void ResetPositioner() {
		actorMap.clear();
		sceneMap.clear();
		sceneMapKey = 1;
		selectedActorFormId = 0;
		positionerEnabled = false;
	}

	void RegisterPositionerFunctions(VirtualMachine* vm) {
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, bool>("IsPositionerEnabled", "AAFDynamicPositioner", IsPositionerEnabled, vm));
		vm->RegisterFunction(
			new NativeFunction1<StaticFunctionTag, void, bool>("SetPositionerState", "AAFDynamicPositioner", SetPositionerState, vm));
		vm->RegisterFunction(
			new NativeFunction2<StaticFunctionTag, void, VMArray<Actor*>, Actor*>("SceneInit", "AAFDynamicPositioner", SceneInit, vm));
		vm->RegisterFunction(
			new NativeFunction2<StaticFunctionTag, void, BSFixedString, VMArray<Actor*>>("AnimationChange", "AAFDynamicPositioner", AnimationChange, vm));
		vm->RegisterFunction(
			new NativeFunction1<StaticFunctionTag, void, VMArray<Actor*>>("SceneEnd", "AAFDynamicPositioner", SceneEnd, vm));
		vm->RegisterFunction(
			new NativeFunction2<StaticFunctionTag, void, BSFixedString, bool>("MoveAxis", "AAFDynamicPositioner", MoveAxis, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, UInt32>("CanMovePosition", "AAFDynamicPositioner", CanMovePosition, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, Actor*>("ChangeSelectedActor", "AAFDynamicPositioner", ChangeSelectedActor, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, Actor*>("GetSelectedActor", "AAFDynamicPositioner", GetSelectedActor, vm));
		vm->RegisterFunction(
			new NativeFunction1<StaticFunctionTag, SpellItem*, bool>("GetHighlightSpell", "AAFDynamicPositioner", GetHighlightSpell, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, void>("ClearSelectedActorOffset", "AAFDynamicPositioner", ClearSelectedActorOffset, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, void>("ShowSelectedSceneOffset", "AAFDynamicPositioner", ShowSelectedSceneOffset, vm));
		vm->RegisterFunction(
			new NativeFunction0<StaticFunctionTag, void>("ShowPositionerMenuNative", "AAFDynamicPositioner", ShowPositionerMenuNative, vm));
	}
}