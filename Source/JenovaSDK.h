
/*-------------------------------------------------------------+
|                                                              |
|                   _________   ______ _    _____              |
|                  / / ____/ | / / __ \ |  / /   |             |
|             __  / / __/ /  |/ / / / / | / / /| |             |
|            / /_/ / /___/ /|  / /_/ /| |/ / ___ |             |
|            \____/_____/_/ |_/\____/ |___/_/  |_|             |
|                                                              |
|                          Jenova SDK                          |
|                   Developed by Hamid.Memar                   |
|                                                              |
+-------------------------------------------------------------*/

#pragma once

/*
	A script's generated proxy defines `self` as a macro for its property accessor, and the
	user's #include of this header sits after that definition. The macro would rewrite this
	header's own uses of the name, so it is set aside for the duration of the header and put
	back afterwards. GCC tolerated the rewriting by accident; Clang and Emscripten do not.
*/
#pragma push_macro("self")
#undef self

// Enable Jenova SDK
#define JENOVA_SDK

// Jenova API Import/Export
#if defined(_WIN32) || defined(_WIN64)
	#define JENOVA_API_EXPORT __declspec(dllexport)
	#define JENOVA_API_IMPORT __declspec(dllimport)
#else
	#define JENOVA_API_EXPORT __attribute__((visibility("default")))
	#define JENOVA_API_IMPORT 
#endif
#ifdef JENOVA_SDK_STATIC
    #define JENOVA_API
#else
	#if defined(JENOVA_STATIC_BUILD)
		#define JENOVA_API
		#define JENOVA_C_API extern "C"
	#elif defined(JENOVA_SDK_BUILD)
		#define JENOVA_API JENOVA_API_EXPORT
		#define JENOVA_C_API extern "C" JENOVA_API_EXPORT
	#else
		#define JENOVA_API JENOVA_API_IMPORT
		#define JENOVA_C_API extern "C" JENOVA_API_IMPORT
	#endif
#endif

// Jenova Utilities
#define JENOVA_EXPORT				extern "C" JENOVA_API_EXPORT
#define JENOVA_CALLBACK				(void*)static_cast<void(*)(void)>([]()

// Jenova API Interface
#define JNVAPI_WRAPPER				static inline
#define JNVAPI_INTERNAL(fn)			virtual fn

/*
	Diagnostics that must stay off the hot path.

	Every guard the SDK adds sits in front of a call scripts make thousands of times a
	frame, so the reporting half is pushed out of line and marked cold. What is left at the
	call site is a load, a compare and a branch the predictor gets right every time.
*/
#if defined(_MSC_VER)
	#define JNVAPI_COLD				static __declspec(noinline)
	#define JENOVA_UNLIKELY(cond)	(cond)
#else
	#define JNVAPI_COLD				static inline __attribute__((noinline, cold))
	#define JENOVA_UNLIKELY(cond)	(__builtin_expect(!!(cond), 0))
#endif

/*
	Where the script called from.

	Jenova preprocesses a script into its cache but emits a #line pointing back at the real
	file, so __FILE__ and __LINE__ inside a script resolve to the path and line the author
	actually wrote. As default arguments these are folded at each call site, which is the only
	way a header-only helper can name its caller.
*/
#if defined(_MSC_VER) && _MSC_VER < 1926
	#define JENOVA_CALLER_FILE		"<unknown file>"
	#define JENOVA_CALLER_LINE		0
#else
	#define JENOVA_CALLER_FILE		__builtin_FILE()
	#define JENOVA_CALLER_LINE		__builtin_LINE()
#endif

// Jenova Configuration Macros
#define JENOVA_TOOL_SCRIPT

// Jenova Profiler Macros
#define JENOVA_SCRIPT_RECORD(n,t)	sentinel::CommitScriptRecord(__FILE__, n, t)

// Jenova Script Block Macros
#define JENOVA_SCRIPT_BEGIN
#define JENOVA_SCRIPT_END

// Jenova Virtual Machine Block Macros
#define JENOVA_VM_BEGIN
#define JENOVA_VM_END

// Jenova ScriptID Macro
#define JENOVA_SCRIPT_ID

// Jenova Property Macro
#ifndef JENOVA_PROPERTY
	#define JENOVA_PROPERTY(pType, pName, pValue, ...) pType pName = pValue;
#endif

// Jenova Signal Macro
#ifndef JENOVA_SIGNAL
	#define JENOVA_SIGNAL(sName, ...) void __jnvsignal__##sName(__VA_ARGS__) {};
#endif

// Jenova Class Name Macro
#ifndef JENOVA_CLASS_NAME
	#define JENOVA_CLASS_NAME(className)
#endif

// Jenova Activator Macro
#ifndef JENOVA_ACTIVATOR
	#define JENOVA_ACTIVATOR(name, regfn, uregfn)					\
		static struct name##SelfActivator {							\
			inline name##SelfActivator() {							\
				RegisterBootEvent((FunctionPtr)&regfn);				\
				RegisterShutdownEvent((FunctionPtr)&uregfn);		\
			}} inline name##_self;
#endif

// C++ Runtime Imports
#ifndef JENOVA_SDK_BUILD
	#include <cstdarg>
	#include <cstdio>
	#include <string>
	#include <thread>
	#include <functional>
#endif

// GodotSDK Imports
#ifndef JENOVA_SDK_BUILD
	#include <Godot/classes/object.hpp>
	#include <Godot/classes/node.hpp>
	#include <Godot/classes/scene_tree.hpp>
	#include <Godot/variant/variant.hpp>
	#include <Godot/variant/string.hpp>
	#include <Godot/variant/string_name.hpp>
	#include <Godot/variant/utility_functions.hpp>
	#include <Godot/classes/global_constants.hpp>
	#include <Godot/classes/font.hpp>
	#include <Godot/classes/texture2d.hpp>
	#include <Godot/classes/material.hpp>
#endif

// Jenova SDK Implementation
namespace jenova::sdk
{
	// Enumerators
	enum class EngineMode
	{
		Editor,
		Debug,
		Runtime,
		Unknown
	};
	enum class FileSystemEvent
	{
		Added,
		Removed,
		Modified,
		RenamedOld,
		RenamedNew
	};
	enum class RuntimeReloadMode
	{
		FullReset,
		HotReload,
		ForceReload
	};
	enum class RuntimeEvent
	{
		/* Must Match to Jenova Runtime */
		Initialized,
		Started,
		Stopped,
		Ready,
		EnterTree,
		ExitTree,
		FrameBegin,
		FrameIdle,
		FrameEnd,
		FramePresent,
		Process,
		PhysicsProcess,
		ReceivedDebuggerMessage
	};
	enum class ClassAccess 
	{
		Core,
		Editor,
		Extension,
		Editor_Extension,
		None
	};

	// Type Definitions
	typedef void*						FunctionPtr;
	typedef void*						NativePtr;
	typedef int64_t						IntPtr;
	typedef uint8_t*					BufferPtr;
	typedef godot::Variant				ObjectPtr;
	typedef const char*					StringPtr;
	typedef const wchar_t*				WideStringPtr;
	typedef godot::Vector2i				ImageSize;
	typedef const char*					MemoryID;
	typedef const char*					VariableID;
	typedef unsigned short				TaskID;
	typedef int64_t						UniqueID;
	typedef int short					DriverResourceID;
	typedef std::function<void()>		FutureFunction;
	typedef std::function<void()>		TaskFunction;
	typedef void*						JenovaSDKInterface;

	// Function Definitions
	typedef void(*RuntimeCallback)(const RuntimeEvent& runtimeEvent, NativePtr dataPtr, size_t dataSize);
	typedef void(*FileSystemCallback)(const godot::String& targetPath, const FileSystemEvent& fsEvent);

	// Caller Structure
	struct Caller
	{
		// Script Caller
		const godot::Object* self = nullptr;

		// Script Context
		const NativePtr context = nullptr;

		// Constructor
		Caller() = default;
		explicit Caller(const godot::Object* object) : self(object), context(nullptr) {}
	};

	// Class Wrappers
	struct Console
	{
		godot::Node* consoleNode = nullptr;
		Console(godot::Node* _consoleNode) : consoleNode(_consoleNode) {};
		inline bool Execute(const godot::String& command)
		{
			if (!consoleNode) return false;
			return bool(consoleNode->call("execute", command));
		}
		inline void Log(const godot::String& logMessage, godot::Color logColor = godot::Color(12, 34, 56))
		{
			if (!consoleNode) return;
			if (logColor == godot::Color(12, 34, 56)) consoleNode->call("log", logMessage);
			else consoleNode->call("logc", logMessage, logColor);
		}
		inline void AddHistory(const godot::String& history)
		{
			if (!consoleNode) return;
			consoleNode->call("add_history", history);
		}
		inline void Error(const godot::String& errorMessage)
		{
			if (!consoleNode) return;
			consoleNode->call("error", errorMessage);
		}
		inline void Flush()
		{
			if (!consoleNode) return;
			consoleNode->call("flush");
		}
		inline bool IsOpen()
		{
			if (!consoleNode) return false;
			return consoleNode->call("is_open");
		}
		inline godot::String GetData()
		{
			return consoleNode->call("get_data");
		}
	};

	// JenovaSDK Interface
	struct JenovaSDK
	{
		// Helpers Utilities
		JNVAPI_INTERNAL(bool IsEditor());
		JNVAPI_INTERNAL(bool IsGame());
		JNVAPI_INTERNAL(EngineMode GetEngineMode());
		JNVAPI_INTERNAL(godot::Node* GetNodeByPath(const godot::String& nodePath));
		JNVAPI_INTERNAL(godot::Node* FindNodeByName(godot::Node* parent, const godot::String& name));
		JNVAPI_INTERNAL(StringPtr GetNodeUniqueID(godot::Node* node));
		JNVAPI_INTERNAL(godot::SceneTree* GetTree());
		JNVAPI_INTERNAL(double GetTime());
		JNVAPI_INTERNAL(void Alert(StringPtr format, va_list args));
		JNVAPI_INTERNAL(godot::String Format(StringPtr format, va_list args));
		JNVAPI_INTERNAL(godot::String Format(WideStringPtr format, va_list args));
		JNVAPI_INTERNAL(void Output(StringPtr format, va_list args));
		JNVAPI_INTERNAL(void Output(WideStringPtr format, va_list args));
		JNVAPI_INTERNAL(void DebugOutput(StringPtr format, va_list args));
		JNVAPI_INTERNAL(void DebugOutput(WideStringPtr format, va_list args));
		JNVAPI_INTERNAL(StringPtr GetCStr(const godot::String& godotStr));
		JNVAPI_INTERNAL(WideStringPtr GetWCStr(const godot::String& godotStr));
		JNVAPI_INTERNAL(ObjectPtr GetObjectPointer(NativePtr obj));
		JNVAPI_INTERNAL(godot::Ref<godot::Font> CreateFontFromBuffer(BufferPtr bufferPtr, size_t bufferSize));
		JNVAPI_INTERNAL(godot::Ref<godot::Texture2D> CreateImageFromBuffer(BufferPtr bufferPtr, size_t bufferSize, StringPtr format, ImageSize size));
		JNVAPI_INTERNAL(godot::Ref<godot::Material> CreateShaderMaterialFromSource(const godot::String& shaderSource));
		JNVAPI_INTERNAL(bool SetClassIcon(const godot::String& className, const godot::Ref<godot::Texture2D> iconImage));
		JNVAPI_INTERNAL(double MatchScaleFactor(double inputSize));
		JNVAPI_INTERNAL(godot::Error CreateSignalCallback(godot::Object* object, const godot::String& signalName, FunctionPtr callbackPtr));
		JNVAPI_INTERNAL(bool CreateDirectoryMonitor(const godot::String& directoryPath));
		JNVAPI_INTERNAL(bool CreateFileMonitor(const godot::String& filePath));
		JNVAPI_INTERNAL(bool RegisterFileMonitorCallback(FileSystemCallback callbackPtr));
		JNVAPI_INTERNAL(bool UnregisterFileMonitorCallback(FileSystemCallback callbackPtr));
		JNVAPI_INTERNAL(bool ForcePushProperties(Caller* caller));
		JNVAPI_INTERNAL(bool ForcePullProperties(Caller* caller));
		JNVAPI_INTERNAL(bool ReloadJenovaRuntime(RuntimeReloadMode reloadMode));
		JNVAPI_INTERNAL(void CreateCheckpoint(const godot::String& checkPointName));
		JNVAPI_INTERNAL(double GetCheckpointTime(const godot::String& checkPointName));
		JNVAPI_INTERNAL(void DeleteCheckpoint(const godot::String& checkPointName));
		JNVAPI_INTERNAL(double GetCheckpointTimeAndDispose(const godot::String& checkPointName));
		JNVAPI_INTERNAL(godot::String GetPackageRepositoryPath(bool globalize = false));
		JNVAPI_INTERNAL(bool RegisterRuntimeCallback(RuntimeCallback callbackPtr));
		JNVAPI_INTERNAL(bool UnregisterRuntimeCallback(RuntimeCallback callbackPtr));

		// Graphic Utilities
		JNVAPI_INTERNAL(NativePtr GetGameWindowHandle());
		JNVAPI_INTERNAL(StringPtr GetRenderingDriverName());
		JNVAPI_INTERNAL(NativePtr GetRenderingDriverResource(DriverResourceID resourceType));

		// Hot-Reloading Utilities (Sakura)
		JNVAPI_INTERNAL(bool SupportsReload());
		JNVAPI_INTERNAL(void PrepareReload(const godot::String& className));
		JNVAPI_INTERNAL(void FinishReload(const godot::String& className));
		JNVAPI_INTERNAL(void Dispose(const godot::String& className));

		// Memory Management Utilities (Anzen)
		JNVAPI_INTERNAL(NativePtr GetGlobalPointer(MemoryID id));
		JNVAPI_INTERNAL(NativePtr SetGlobalPointer(MemoryID id, NativePtr ptr));
		JNVAPI_INTERNAL(void DeleteGlobalPointer(MemoryID id));
		JNVAPI_INTERNAL(NativePtr AllocateGlobalMemory(MemoryID id, size_t size));
		JNVAPI_INTERNAL(void FreeGlobalMemory(MemoryID id));

		// Global Variable Storage Utilities (Anzen)
		JNVAPI_INTERNAL(godot::Variant GetGlobalVariable(VariableID id));
		JNVAPI_INTERNAL(void SetGlobalVariable(VariableID id, godot::Variant var));
		JNVAPI_INTERNAL(void ClearGlobalVariables());

		// Runtime Dispatcher Utilities
		JNVAPI_INTERNAL(UniqueID QueueFunction(FutureFunction function, int milliseconds));
		JNVAPI_INTERNAL(UniqueID QueueFunction(FutureFunction function, double seconds));
		JNVAPI_INTERNAL(bool IsFunctionInQueue(UniqueID functionID));
		JNVAPI_INTERNAL(bool AbortQueuedFunction(UniqueID functionID));

		// Task System Utilities
		JNVAPI_INTERNAL(TaskID InitiateTask(TaskFunction function));
		JNVAPI_INTERNAL(bool IsTaskComplete(TaskID taskID));
		JNVAPI_INTERNAL(void ClearTask(TaskID taskID));

		// C Scripting Utilities (Clektron)
		JNVAPI_INTERNAL(bool ExecuteScript(StringPtr ctronScript, bool noEntrypoint = false));
		JNVAPI_INTERNAL(bool ExecuteScriptFromFile(StringPtr ctronScriptFile, bool noEntrypoint = false));
		JNVAPI_INTERNAL(bool BindSymbol(FunctionPtr symbolPtr, StringPtr symbolName, StringPtr returnType, int paramCount, va_list args));
		JNVAPI_INTERNAL(bool ExecuteScript(const godot::String& ctronScript, bool noEntrypoint = false));
		JNVAPI_INTERNAL(bool ExecuteScriptFromFile(const godot::String& ctronScriptFile, bool noEntrypoint = false));
		JNVAPI_INTERNAL(bool BindSymbol(FunctionPtr symbolPtr, const godot::String& symbolName, const godot::String& returnType, int paramCount, va_list args));

		// Profiling Utilities (Sentinel)
		JNVAPI_INTERNAL(bool IsProfilerEnabled());
		JNVAPI_INTERNAL(bool CommitRecord(StringPtr recordName, double recordTime));
		JNVAPI_INTERNAL(bool CommitScriptRecord(StringPtr fileName, StringPtr recordName, double recordTime));

		// Interface Validator
		static bool ValidateInterface(NativePtr bridgePtr)
		{
			if (!bridgePtr) return false;
			return true;
		}
	};

	// JenovaSDK Singleton
	extern JenovaSDK* bridge;

	// JenovaSDK Interface Management
	JENOVA_C_API JenovaSDKInterface GetSDKInterface();
	JENOVA_C_API FunctionPtr GetSDKFunction(StringPtr sdkFunctionName);

	// Helpers Utilities :: Wrappers
	JNVAPI_WRAPPER bool IsEditor()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->IsEditor();
	}
	JNVAPI_WRAPPER bool IsGame()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->IsGame();
	}
	JNVAPI_WRAPPER EngineMode GetEngineMode()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return EngineMode::Unknown;
		return bridge->GetEngineMode();
	}
	JNVAPI_WRAPPER godot::Node* GetNodeByPath(const godot::String& nodePath)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetNodeByPath(nodePath);
	}
	JNVAPI_WRAPPER godot::Node* FindNodeByName(godot::Node* parent, const godot::String& name)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->FindNodeByName(parent, name);
	}
	JNVAPI_WRAPPER StringPtr GetNodeUniqueID(godot::Node* node)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetNodeUniqueID(node);
	}
	JNVAPI_WRAPPER godot::SceneTree* GetTree()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetTree();
	}
	JNVAPI_WRAPPER double GetTime()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0.0;
		return bridge->GetTime();
	}
	JNVAPI_WRAPPER void Alert(StringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		va_list args;
		va_start(args, format);
		bridge->Alert(format, args);
		va_end(args);
	}
	JNVAPI_WRAPPER godot::String Format(StringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return godot::String();
		va_list args;
		va_start(args, format);
		godot::String result = bridge->Format(format, args);
		va_end(args);
		return result;
	}
	JNVAPI_WRAPPER godot::String Format(WideStringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return godot::String();
		va_list args;
		va_start(args, format);
		godot::String result = bridge->Format(format, args);
		va_end(args);
		return result;
	}
	JNVAPI_WRAPPER void Output(StringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		va_list args;
		va_start(args, format);
		bridge->Output(format, args);
		va_end(args);
	}
	JNVAPI_WRAPPER void Output(WideStringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		va_list args;
		va_start(args, format);
		bridge->Output(format, args);
		va_end(args);
	}
	JNVAPI_WRAPPER void DebugOutput(StringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		va_list args;
		va_start(args, format);
		bridge->DebugOutput(format, args);
		va_end(args);
	}
	JNVAPI_WRAPPER void DebugOutput(WideStringPtr format, ...)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		va_list args;
		va_start(args, format);
		bridge->DebugOutput(format, args);
		va_end(args);
	}
	JNVAPI_WRAPPER StringPtr GetCStr(const godot::String& godotStr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetCStr(godotStr);
	}
	JNVAPI_WRAPPER WideStringPtr GetWCStr(const godot::String& godotStr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetWCStr(godotStr);
	}
	JNVAPI_WRAPPER ObjectPtr GetObjectPointer(NativePtr obj)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetObjectPointer(obj);
	}
	JNVAPI_WRAPPER godot::Ref<godot::Font> CreateFontFromBuffer(BufferPtr bufferPtr, size_t bufferSize)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->CreateFontFromBuffer(bufferPtr, bufferSize);
	}
	JNVAPI_WRAPPER godot::Ref<godot::Texture2D> CreateImageFromBuffer(BufferPtr bufferPtr, size_t bufferSize, StringPtr format, ImageSize size = ImageSize())
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->CreateImageFromBuffer(bufferPtr, bufferSize, format, size);
	}
	JNVAPI_WRAPPER godot::Ref<godot::Material> CreateShaderMaterialFromSource(const godot::String& shaderSource)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->CreateShaderMaterialFromSource(shaderSource);
	}
	JNVAPI_WRAPPER bool SetClassIcon(const godot::String& className, const godot::Ref<godot::Texture2D> iconImage)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->SetClassIcon(className, iconImage);
	}
	JNVAPI_WRAPPER double MatchScaleFactor(double inputSize)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0.0;
		return bridge->MatchScaleFactor(inputSize);
	}
	JNVAPI_WRAPPER godot::Error CreateSignalCallback(godot::Object* object, const godot::String& signalName, FunctionPtr callbackPtr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return godot::ERR_INVALID_PARAMETER;
		return bridge->CreateSignalCallback(object, signalName, callbackPtr);
	}
	JNVAPI_WRAPPER bool CreateDirectoryMonitor(const godot::String& directoryPath)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->CreateDirectoryMonitor(directoryPath);
	}
	JNVAPI_WRAPPER bool CreateFileMonitor(const godot::String& filePath)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->CreateFileMonitor(filePath);
	}
	JNVAPI_WRAPPER bool RegisterFileMonitorCallback(FileSystemCallback callbackPtr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->RegisterFileMonitorCallback(callbackPtr);
	}
	JNVAPI_WRAPPER bool UnregisterFileMonitorCallback(FileSystemCallback callbackPtr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->UnregisterFileMonitorCallback(callbackPtr);
	}
	JNVAPI_WRAPPER bool ForcePushProperties(Caller* caller)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->ForcePushProperties(caller);
	}
	JNVAPI_WRAPPER bool ForcePullProperties(Caller* caller)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->ForcePullProperties(caller);
	}
	JNVAPI_WRAPPER bool ReloadJenovaRuntime(RuntimeReloadMode reloadMode)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->ReloadJenovaRuntime(reloadMode);
	}
	JNVAPI_WRAPPER void CreateCheckpoint(const godot::String& checkPointName)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->CreateCheckpoint(checkPointName);
	}
	JNVAPI_WRAPPER double GetCheckpointTime(const godot::String& checkPointName)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0.0;
		return bridge->GetCheckpointTime(checkPointName);
	}
	JNVAPI_WRAPPER void DeleteCheckpoint(const godot::String& checkPointName)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->DeleteCheckpoint(checkPointName);
	}
	JNVAPI_WRAPPER double GetCheckpointTimeAndDispose(const godot::String& checkPointName)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0.0;
		return bridge->GetCheckpointTimeAndDispose(checkPointName);
	}
	JNVAPI_WRAPPER godot::String GetPackageRepositoryPath(bool globalize = false)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return godot::String();
		return bridge->GetPackageRepositoryPath(globalize);
	}
	JNVAPI_WRAPPER bool RegisterRuntimeCallback(RuntimeCallback callbackPtr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->RegisterRuntimeCallback(callbackPtr);
	}
	JNVAPI_WRAPPER bool UnregisterRuntimeCallback(RuntimeCallback callbackPtr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->UnregisterRuntimeCallback(callbackPtr);
	}

	// Graphic Utilities :: Wrappers
	JNVAPI_WRAPPER NativePtr GetGameWindowHandle()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetGameWindowHandle();
	}
	JNVAPI_WRAPPER StringPtr GetRenderingDriverName()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetRenderingDriverName();
	}
	JNVAPI_WRAPPER NativePtr GetRenderingDriverResource(DriverResourceID resourceType)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetRenderingDriverResource(resourceType);
	}

	// Hot-Reloading Utilities (Sakura) :: Wrappers
	namespace sakura
	{
		JNVAPI_WRAPPER bool SupportsReload()
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->SupportsReload();
		}
		JNVAPI_WRAPPER void PrepareReload(const godot::String& className)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return;
			bridge->PrepareReload(className);
		}
		JNVAPI_WRAPPER void FinishReload(const godot::String& className)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return;
			bridge->FinishReload(className);
		}
		JNVAPI_WRAPPER void Dispose(const godot::String& className)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return;
			bridge->Dispose(className);
		}
	}

	// Memory Management Utilities (Anzen) :: Wrappers
	JNVAPI_WRAPPER NativePtr GetGlobalPointer(MemoryID id)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->GetGlobalPointer(id);
	}
	JNVAPI_WRAPPER NativePtr SetGlobalPointer(MemoryID id, NativePtr ptr)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->SetGlobalPointer(id, ptr);
	}
	JNVAPI_WRAPPER void DeleteGlobalPointer(MemoryID id)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->DeleteGlobalPointer(id);
	}
	JNVAPI_WRAPPER NativePtr AllocateGlobalMemory(MemoryID id, size_t size)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return nullptr;
		return bridge->AllocateGlobalMemory(id, size);
	}
	JNVAPI_WRAPPER void FreeGlobalMemory(MemoryID id)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->FreeGlobalMemory(id);
	}

	// Global Variable Storage Utilities (Anzen) :: Wrappers
	JNVAPI_WRAPPER godot::Variant GetGlobalVariable(VariableID id)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return godot::Variant();
		return bridge->GetGlobalVariable(id);
	}
	JNVAPI_WRAPPER void SetGlobalVariable(VariableID id, godot::Variant var)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->SetGlobalVariable(id, var);
	}
	JNVAPI_WRAPPER void ClearGlobalVariables()
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->ClearGlobalVariables();
	}

	// Runtime Dispatcher Utilities :: Wrappers
	JNVAPI_WRAPPER UniqueID QueueFunction(FutureFunction function, int milliseconds = 10)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0;
		return bridge->QueueFunction(function, milliseconds);
	}
	JNVAPI_WRAPPER UniqueID QueueFunction(FutureFunction function, double seconds = 0.01)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return 0;
		return bridge->QueueFunction(function, seconds);
	}
	JNVAPI_WRAPPER bool IsFunctionInQueue(UniqueID functionID)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->IsFunctionInQueue(functionID);
	}
	JNVAPI_WRAPPER bool AbortQueuedFunction(UniqueID functionID)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->AbortQueuedFunction(functionID);
	}

	// Task System Utilities :: Wrappers
	JNVAPI_WRAPPER TaskID InitiateTask(TaskFunction function)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return TaskID();
		return bridge->InitiateTask(function);
	}
	JNVAPI_WRAPPER bool IsTaskComplete(TaskID taskID)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return false;
		return bridge->IsTaskComplete(taskID);
	}
	JNVAPI_WRAPPER void ClearTask(TaskID taskID)
	{
		if (!JenovaSDK::ValidateInterface(bridge)) return;
		bridge->ClearTask(taskID);
	}

	// C Scripting Utilities (Clektron)
	namespace clektron
	{
		JNVAPI_WRAPPER bool ExecuteScript(StringPtr ctronScript, bool noEntrypoint = false)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->ExecuteScript(ctronScript, noEntrypoint);
		}
		JNVAPI_WRAPPER bool ExecuteScriptFromFile(StringPtr ctronScriptFile, bool noEntrypoint = false)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->ExecuteScript(ctronScriptFile, noEntrypoint);
		}
		JNVAPI_WRAPPER bool BindSymbol(FunctionPtr symbolPtr, StringPtr symbolName, StringPtr returnType = "void", int paramCount = 0, ...)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			va_list args;
			va_start(args, paramCount);
			bool result = bridge->BindSymbol(symbolPtr, symbolName, returnType, paramCount, args);
			va_end(args);
			return result;
		}
		JNVAPI_WRAPPER bool ExecuteScript(const godot::String& ctronScript, bool noEntrypoint = false)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->ExecuteScript(ctronScript, noEntrypoint);
		}
		JNVAPI_WRAPPER bool ExecuteScriptFromFile(const godot::String& ctronScriptFile, bool noEntrypoint = false)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->ExecuteScript(ctronScriptFile, noEntrypoint);
		}
		JNVAPI_WRAPPER bool BindSymbol(FunctionPtr symbolPtr, const godot::String& symbolName, const godot::String& returnType, int paramCount = 0, ...)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			va_list args;
			va_start(args, paramCount);
			bool result = bridge->BindSymbol(symbolPtr, symbolName, returnType, paramCount, args);
			va_end(args);
			return result;
		}
	}

	// Profiling Utilities (Sentinel)
	namespace sentinel
	{
		JNVAPI_WRAPPER bool IsProfilerEnabled()
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->IsProfilerEnabled();
		}
		JNVAPI_WRAPPER bool CommitRecord(StringPtr recordName, double recordTime = 0)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->CommitRecord(recordName, recordTime);
		}
		JNVAPI_WRAPPER bool CommitScriptRecord(StringPtr fileName, StringPtr recordName, double recordTime)
		{
			if (!JenovaSDK::ValidateInterface(bridge)) return false;
			return bridge->CommitScriptRecord(fileName, recordName, recordTime);
		}
	}

	/*
		Diagnostics :: Wrappers

		Reported straight through the engine rather than through the SDK bridge, deliberately.
		Routing this to the runtime would mean adding a method to the JenovaSDK interface, and
		a module built against a header with that method but loaded by an older runtime would
		call one vtable slot past the end of the real vtable -- an undefined jump, which is a
		worse failure than the silence this whole effort exists to remove. Nothing here depends
		on the runtime's version.

		push_error is also what reaches the editor's Output dock: the running game is a child
		process, and only what crosses the debugger connection is shown there. A plain write to
		stderr lands in the terminal that launched the editor, where nobody is looking.
	*/
	JNVAPI_WRAPPER void ReportScriptError(StringPtr scope, StringPtr message, StringPtr file, int line)
	{
		godot::UtilityFunctions::push_error(godot::String("[Jenova C++ Script] [") + godot::String(scope ? scope : "Unknown") + "] "
			+ godot::String(message ? message : "Unspecified Error.")
			+ "\n  At : " + godot::String(file ? file : "<unknown file>") + ":" + godot::String::num_int64(line));
	}

	/*
		Reached only when a script is one instruction away from dereferencing null.

		This is the single most common way a C++ script kills the process: a `self` property
		declared but never assigned, usually because the _enter_tree that assigns it was
		commented out or renamed. The engine calls _ready, GetSelf hands back null, the very
		next get_node() faults, and on POSIX that used to be the end of it -- no message, no
		stack, just a window that closes. Naming the mistake here costs the caller a
		not-taken branch.
	*/
	JNVAPI_COLD godot::Object* ReportInvalidSelfVariant(const godot::Variant& badSelf, StringPtr file, int line)
	{
		const godot::CharString actualType = godot::Variant::get_type_name(badSelf.get_type()).utf8();
		char message[768];
		snprintf(message, sizeof(message),
			"The script's `self` holds %s and does not resolve to a live Object, so GetSelf<T>() returned null and the next member access will crash.\n"
			"  `self` is only valid once it has been assigned from the Caller the engine passes in. Assign it before any other callback runs:\n"
			"      void _enter_tree(Caller* instance) { self = GetSelf<YourNodeType>(instance); }\n"
			"  A `self` left at its JENOVA_PROPERTY default reports as Int or Nil here. A `self` that reports as Object instead points at a node that has already been freed.",
			actualType.get_data());
		ReportScriptError("GetSelf", message, file, line);
		return nullptr;
	}
	JNVAPI_COLD godot::Node* ReportMissingNode(StringPtr scope, const godot::String& nodeQuery, StringPtr file, int line)
	{
		const godot::CharString query = nodeQuery.utf8();
		char message[768];
		snprintf(message, sizeof(message),
			"No node matched \"%s\", so a null was returned and the next member access will crash.\n"
			"  The node was renamed or moved, the path is relative to the wrong parent, or the lookup ran before the node entered the tree.",
			query.get_data());
		ReportScriptError(scope, message, file, line);
		return nullptr;
	}
	JNVAPI_COLD void ReportMissingGlobal(StringPtr scope, MemoryID id, StringPtr file, int line)
	{
		char message[512];
		snprintf(message, sizeof(message),
			"No global memory is registered under the id \"%s\". Allocate it with AllocateGlobalMemory or SetGlobalPointer before reading it.",
			id ? id : "<null>");
		ReportScriptError(scope, message, file, line);
	}
	JNVAPI_COLD godot::Object* ReportInvalidSelfCaller(Caller* badCaller, StringPtr file, int line)
	{
		ReportScriptError("GetSelf", badCaller
			? "The Caller the engine passed in carries a null owner, so GetSelf<T>() returned null and the next member access will crash. The node this script is attached to is being destroyed, or the call arrived before the script was bound to it."
			: "GetSelf<T>() was given a null Caller, so it returned null and the next member access will crash. Only the pointer a script callback receives as its first parameter is a valid Caller.",
			file, line);
		return nullptr;
	}

	// Template Helpers
	template <typename T> T* GetSelf(Caller* caller, StringPtr _file = JENOVA_CALLER_FILE, int _line = JENOVA_CALLER_LINE)
	{
		if (JENOVA_UNLIKELY(!caller || !caller->self)) return static_cast<T*>(ReportInvalidSelfCaller(caller, _file, _line));
		return (T*)(caller->self);
	}
	/*
		Scripts call this at the top of every callback to reach their own node, so it is on
		the hot path of every engine-to-C++ call.

		Taking the Variant by value copied it. godot-cpp's Object::cast_to then built a
		StringName from the class name, asked ClassDB for its tag and performed a checked
		cast, which is three engine round trips to re-discover a type the script already
		named. The owner never changes type, and the Caller* overload above has always been
		an unchecked cast, so this one matches it.

		The guard below deliberately tests the result of that same cast rather than asking
		the Variant for its type first: get_type() is another call through the extension
		interface, and the conversion already yields null for anything that is not a live
		object. What is added at the call site is a test and a branch on a register that is
		already loaded. Naming the type costs a round trip, so it happens in the cold path,
		once, on the way to an error message.
	*/
	template <typename T> T* GetSelf(const godot::Variant& self, StringPtr _file = JENOVA_CALLER_FILE, int _line = JENOVA_CALLER_LINE)
	{
		godot::Object* selfObject = static_cast<godot::Object*>(self);
		if (JENOVA_UNLIKELY(selfObject == nullptr)) return static_cast<T*>(ReportInvalidSelfVariant(self, _file, _line));
		return static_cast<T*>(selfObject);
	}
	template <typename T> T* GetNode(const godot::String& nodePath, StringPtr _file = JENOVA_CALLER_FILE, int _line = JENOVA_CALLER_LINE)
	{
		godot::Node* node = GetNodeByPath(nodePath);
		if (JENOVA_UNLIKELY(node == nullptr)) return static_cast<T*>(ReportMissingNode("GetNode", nodePath, _file, _line));
		return static_cast<T*>(node);
	}
	template <typename T> T* FindNode(godot::Node* parent, const godot::String& nodeName, StringPtr _file = JENOVA_CALLER_FILE, int _line = JENOVA_CALLER_LINE)
	{
		godot::Node* node = FindNodeByName(parent, nodeName);
		if (JENOVA_UNLIKELY(node == nullptr)) return static_cast<T*>(ReportMissingNode("FindNode", nodeName, _file, _line));
		return static_cast<T*>(node);
	}
	template <typename T> T* GlobalPointer(MemoryID id)
	{
		return static_cast<T*>(GetGlobalPointer(id));
	}
	template <typename T> T GlobalGet(MemoryID id)
	{
		// An id that was never allocated faults on the dereference below. The fault is left
		// exactly where it was -- inventing a default T here would hide a real bug behind a
		// plausible zero -- but it no longer happens without a word about which id it was.
		void* globalPointer = GetGlobalPointer(id);
		if (JENOVA_UNLIKELY(globalPointer == nullptr)) ReportMissingGlobal("GlobalGet", id, JENOVA_CALLER_FILE, JENOVA_CALLER_LINE);
		return *(static_cast<T*>(globalPointer));
	}
	template <typename T> void GlobalSet(MemoryID id, const T& newValue)
	{
		T* ptr = static_cast<T*>(GetGlobalPointer(id));
		if (ptr) *ptr = newValue;
	}
	template <typename T> T GlobalVariable(VariableID id)
	{
		return T(GetGlobalVariable(id));
	}
	template<typename T> bool IsValidObject(const godot::Variant& variant)
	{
		if (variant.get_type() != godot::Variant::OBJECT) return false;
		godot::Object* obj = variant;
		return obj != nullptr && godot::Object::cast_to<T>(obj) != nullptr;
	}
	template <typename T> T* GetObjectFromIntPtr(IntPtr ptr)
	{
		return reinterpret_cast<T*>(ptr);
	}
	template <typename T> T* GetObjectFromIntPtr(godot::Variant variantPtr)
	{
		return reinterpret_cast<T*>(IntPtr(variantPtr));
	}
	template <typename T> T* Instantiate()
	{
		godot::String typeName(typeid(T).name());
		typeName = typeName.substr(typeName.find("::") + 2);
		return godot::Object::cast_to<T>(godot::ClassDB::instantiate(typeName));
	}
	template <typename T> T* Instantiate(const godot::String& className)
	{
		return godot::Object::cast_to<T>(godot::ClassDB::instantiate(className));
	}
	template <typename T> godot::Ref<T> InstantiateAsRef()
	{
		godot::String typeName(typeid(T).name());
		typeName = typeName.substr(typeName.find("::") + 2);
		return godot::Ref<T>(godot::Object::cast_to<T>(godot::ClassDB::instantiate(typeName)));
	}
	template <typename T> godot::Ref<T> InstantiateAsRef(const godot::String& className)
	{
		return godot::Ref<T>(godot::Object::cast_to<T>(godot::ClassDB::instantiate(className)));
	}

	// Internal Module Functions
	bool RegisterBootEvent(jenova::sdk::FunctionPtr funcionPtr, int index = -1);
	bool RegisterShutdownEvent(jenova::sdk::FunctionPtr funcionPtr, int index = -1);
	bool UnregisterBootEvent(jenova::sdk::FunctionPtr funcionPtr);
	bool UnregisterShutdownEvent(jenova::sdk::FunctionPtr funcionPtr);
}

#pragma pop_macro("self")
