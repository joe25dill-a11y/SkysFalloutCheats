#pragma once
// Lean NVSE Plugin API surface for Sky's Fallout Cheats.
// Self-contained — does not pull the full xNVSE source tree.
// Layout verified against xNVSE 6.4.8 PluginAPI.h.

#include <cstdint>

using UInt8  = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using SInt32 = std::int32_t;

class TESObjectREFR;
struct CommandInfo;
enum CommandReturnType : UInt32;

using PluginHandle = UInt32;

enum : UInt32
{
	kPluginHandle_Invalid = 0xFFFFFFFFu
};

enum : UInt32
{
	kInterface_Serialization = 0,
	kInterface_Console,
	kInterface_Messaging,
	kInterface_CommandTable,
	kInterface_StringVar,
	kInterface_ArrayVar,
	kInterface_Script,
	kInterface_Data,
	kInterface_EventManager,
	kInterface_Logging,
	kInterface_PlayerControls,
	kInterface_Max
};

struct ExpressionEvaluatorUtils;

struct NVSEInterface
{
	UInt32 nvseVersion;
	UInt32 runtimeVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	bool (*RegisterCommand)(CommandInfo* info);
	void (*SetOpcodeBase)(UInt32 opcode);
	void* (*QueryInterface)(UInt32 id);
	PluginHandle (*GetPluginHandle)(void);
	bool (*RegisterTypedCommand)(CommandInfo* info, CommandReturnType retnType);
	const char* (*GetRuntimeDirectory)();
	UInt32 isNogore;
	void (*InitExpressionEvaluatorUtils)(ExpressionEvaluatorUtils* utils);
	bool (*RegisterTypedCommandVersion)(CommandInfo* info, CommandReturnType retnType, UInt32 requiredPluginVersion);
};

struct NVSEConsoleInterface
{
	enum { kVersion = 3 };
	UInt32 version;
	bool (*RunScriptLine)(const char* buf, TESObjectREFR* object);
	bool (*RunScriptLine2)(const char* buf, TESObjectREFR* callingRefr, bool bSuppressConsoleOutput);
};

struct PluginInfo
{
	enum { kInfoVersion = 1 };
	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

// Lean messaging surface (xNVSE PluginAPI.h layout — kVersion 4).
struct NVSEMessagingInterface
{
	struct Message {
		const char* sender;
		UInt32 type;
		UInt32 dataLen;
		void* data;
	};

	typedef void (*EventCallback)(Message* msg);

	enum { kVersion = 4 };

	enum {
		kMessage_PostLoad = 0,
		kMessage_ExitGame,
		kMessage_ExitToMainMenu,
		kMessage_LoadGame,
		kMessage_SaveGame,
		kMessage_ScriptPrecompile,
		kMessage_PreLoadGame,
		kMessage_ExitGame_Console,
		kMessage_PostLoadGame,
		kMessage_PostPostLoad,
		kMessage_RuntimeScriptError,
		kMessage_DeleteGame,
		kMessage_RenameGame,
		kMessage_RenameNewGame,
		kMessage_NewGame,
		kMessage_DeleteGameName,
		kMessage_RenameGameName,
		kMessage_RenameNewGameName,
		kMessage_DeferredInit,
		kMessage_ClearScriptDataCache,
		kMessage_MainGameLoop,
		kMessage_ScriptCompile,
		kMessage_EventListDestroyed,
		kMessage_PostQueryPlugins,
		kMessage_OnFramePresent,
		kMessage_ReloadConfig,
		kMessage_OnRefSet3D,
		kMessage_OnRefUnset3D,
		kMessage_OnRefAttach,
		kMessage_OnCellStateChange,
		kMessage_OnCellRefsLoaded,
		kMessage_OnNonPersistentFormLoad,
		kMessage_OnNonPersistentFormUnload,
	};

	UInt32 version;
	bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
	bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data, UInt32 dataLen, const char* receiver);
};
