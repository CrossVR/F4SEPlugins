#include "Global.h"

bool IsPrewarNora(UInt32 formId, UInt32 baseFormId) {
	// BaseForm�� ID�� ��ġ�ϴ� ���۷��� NPC�� �� �̻� ������ �� �����Ƿ�
	// �ٴҶ� ���۷��� NPC���� Ȯ���Ͽ� ����
	return formId != 0xA7D38 && baseFormId == 0xA7D35;
}

bool IsAAFDoppelganger(UInt32 formId) {
	if (!*g_dataHandler)
		return false;

	static TESForm* aaf_doppForm = GetFormFromIdentifier("AAF.esm|72E2");
	if (!aaf_doppForm)
		return false;
	return formId == aaf_doppForm->formID;
}

bool IsAAFDoppelganger(Actor* actor, bool isFemale) {
	if (!actor || !actor->baseForm)
		return false;

	if (isFemale) {
		if (IsPrewarNora(actor->formID, actor->baseForm->formID) || IsAAFDoppelganger(actor->baseForm->formID))
			return true;
	}
	else if (IsAAFDoppelganger(actor->baseForm->formID))
		return true;

	return false;
}

EventResult SceneActionEventReceiver::ReceiveEvent(TESSceneActionEvent* evn, void* dispatcher) {
	static TESForm* aaf_DoppelgangerWaitQuest = GetFormFromIdentifier("AAF.esm|72E6");
	if (!aaf_DoppelgangerWaitQuest || aaf_DoppelgangerWaitQuest->formID != evn->sceneFormID || evn->actionIndex != 1)
		return kEvent_Continue;

	if (*g_player && (*g_player)->parentCell) {
		for (UInt32 ii = 0; ii < (*g_player)->parentCell->objectList.count; ii++) {
			TESObjectREFR* ref = (*g_player)->parentCell->objectList.entries[ii];
			Actor* actorRef = DYNAMIC_CAST(ref, TESObjectREFR, Actor);
			if (actorRef) {
				bool isPlayerFemale = isActorFemale(*g_player);
				if (IsAAFDoppelganger(actorRef, isPlayerFemale)) {
					SetActorRefScale(actorRef, GetPlayerScale(), isPlayerFemale);
					break;
				}
			}
		}
	}
	return kEvent_Continue;
}