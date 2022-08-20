// F4SE
#include "common/IDebugLog.h"  // IDebugLog

#include <shlobj.h>	// CSIDL_MYCODUMENTS

// User Defined Header
#include "Global.h"

PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface* g_messaging = nullptr;
F4SEScaleformInterface* g_scaleform = nullptr;
F4SEPapyrusInterface*	g_papyrus = nullptr;

RelocPtr <uintptr_t> TESIdleForm_VTable(0x02CB8FA8);
RelocPtr <TESIdleForm**> g_IdleArr(0x58D2ED0);

std::vector<std::string> g_pluginVec;
CaseInsensitiveMap<CaseInsensitiveMap<TESIdleForm*>> g_idleAnimMap;
CaseInsensitiveMap<std::vector<std::string>> g_customPoseMap;

bool IsValidIdleForm(void* ptr) {
	if (!ptr)
		return false;

	uintptr_t* vtablePtr = reinterpret_cast<uintptr_t*>(ptr);
	if (*vtablePtr == TESIdleForm_VTable.GetUIntPtr())
		return true;
	return false;
}

void InitializeIdleMap() {
	for (UInt32 ii = 0; ; ii++) {
		TESIdleForm* idleForm = (*g_IdleArr)[ii];
		if (!idleForm || !IsValidIdleForm(idleForm))
			break;

		if (!idleForm->unk08)
			continue;

		ModInfo* modInfo = idleForm->unk08->entries[0];
		if (!modInfo)
			continue;

		if (!idleForm->formEditorID)
			continue;

		auto it = g_idleAnimMap.find(modInfo->name);
		if (it == g_idleAnimMap.end()) {
			auto in_res = g_idleAnimMap.insert(std::make_pair(modInfo->name, CaseInsensitiveMap<TESIdleForm*>()));
			it = in_res.first;
		}

		it->second.insert(std::make_pair(idleForm->formEditorID, idleForm));
	}
}

void InitializeConfig() {
	if (ConfigReader::ShouldReadConfig())
		g_pluginVec = ConfigReader::ReadConfig();

	for (std::string& plugin : g_pluginVec) {
		if (!ConfigReader::ShouldReadPluginPoseConfig(plugin))
			continue;

		std::vector<std::string> poses = ConfigReader::ReadPluginPoseConfig(plugin);
		if (poses.empty()) {
			g_customPoseMap.erase(plugin);
			continue;
		}

		auto it = g_customPoseMap.find(plugin);
		if (it == g_customPoseMap.end())
			g_customPoseMap.insert(std::make_pair(plugin, poses));
		else
			g_customPoseMap[plugin] = poses;
	}
}

void OnF4SEMessage(F4SEMessagingInterface::Message* msg) {
	switch (msg->type) {
	case F4SEMessagingInterface::kMessage_GameDataReady:
		break;

	case F4SEMessagingInterface::kMessage_GameLoaded:
		InitializeIdleMap();
		ScaleformManager::TullPoserHotKeyMenu::RegisterMenu();
		break;

	case F4SEMessagingInterface::kMessage_PreLoadGame:
		InitializeConfig();
		ScaleformManager::ClearSelectedVars();

		break;
	}
}

bool RegisterScaleform(GFxMovieView* view, GFxValue* f4se_root) {
	RegisterFunction<ScaleformManager::InitializeHandler>(f4se_root, view->movieRoot, "Initialize");
	RegisterFunction<ScaleformManager::CloseHandler>(f4se_root, view->movieRoot, "Close");
	RegisterFunction<ScaleformManager::GetPluginListHandler>(f4se_root, view->movieRoot, "GetPluginList");
	RegisterFunction<ScaleformManager::SelectPluginHandler>(f4se_root, view->movieRoot, "SelectPlugin");
	RegisterFunction<ScaleformManager::GetPoseListHandler>(f4se_root, view->movieRoot, "GetPoseList");
	RegisterFunction<ScaleformManager::SelectPoseHandler>(f4se_root, view->movieRoot, "SelectPose");
	return true;
}

bool RegisterFuncs(VirtualMachine* vm) {
	vm->RegisterFunction(new NativeFunction0<StaticFunctionTag, void>("ShowMenu", "TullPoserHotKey", PapyrusManager::ShowMenu, vm));
	return true;
}

/* Plugin Query */
extern "C" {
	bool F4SEPlugin_Query(const F4SEInterface* f4se, PluginInfo* info) {
		std::string logPath{ "\\My Games\\Fallout4\\F4SE\\" PLUGIN_NAME ".log" };
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, logPath.c_str());
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = PLUGIN_VERSION;

		if (f4se->runtimeVersion != RUNTIME_VERSION_1_10_163) {
			_MESSAGE("unsupported runtime version %d", f4se->runtimeVersion);
			return false;
		}

		g_pluginHandle = f4se->GetPluginHandle();

		// Get the messaging interface
		g_messaging = (F4SEMessagingInterface*)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_FATALERROR("couldn't get messaging interface");
			return false;
		}

		g_scaleform = (F4SEScaleformInterface*)f4se->QueryInterface(kInterface_Scaleform);
		if (!g_scaleform) {
			_FATALERROR("couldn't get scaleform interface");
			return false;
		}

		g_papyrus = (F4SEPapyrusInterface*)f4se->QueryInterface(kInterface_Papyrus);
		if (!g_papyrus) {
			_FATALERROR("couldn't get papyrus interface");
			return false;
		}

		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface* f4se) {
		_MESSAGE("%s Loaded", PLUGIN_NAME);

		InitializeConfig();

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "F4SE", OnF4SEMessage);

		if (g_scaleform)
			g_scaleform->Register(PLUGIN_NAME, RegisterScaleform);

		if (g_papyrus)
			g_papyrus->Register(RegisterFuncs);

		return true;
	}
};