#pragma once

/*-------------------------------------------------------------+
|                                                              |
|                   _________   ______ _    _____              |
|                  / / ____/ | / / __ \ |  / /   |             |
|             __  / / __/ /  |/ / / / / | / / /| |             |
|            / /_/ / /___/ /|  / /_/ /| |/ / ___ |             |
|            \____/_____/_/ |_/\____/ |___/_/  |_|             |
|                                                              |
|                        Jenova Runtime                        |
|                   Developed by Hamid.Memar                   |
|                                                              |
+-------------------------------------------------------------*/

// Jenova SDK
#include "Jenova.hpp"

// Jenova Interpreter Definitions
class JenovaInterpreter
{
public:
    // Module Management API
    static void BootInterpreter();
    static bool InitializeInterpreter();
    static bool IsInterpreterInitialized();
    static bool ReleaseInterpreter();
    static bool LoadModule(const uint8_t* moduleDataPtr, size_t moduleSize, const jenova::SerializedData& metaData);
    static bool LoadModule(const jenova::BuildResult& buildResult);
    static bool ReloadModule(const uint8_t* moduleDataPtr, size_t moduleSize, const jenova::SerializedData& metaData);
    static bool ReloadModule(const jenova::BuildResult& buildResult);
    static bool UnloadModule(const jenova::ModuleUnloadStage& unloadStage);
    static uint64_t GetModuleGeneration();
    static jenova::PropertySetMethod GetPropertySetMethod();
    static bool LoadDebugSymbol(const std::string symbolFilePath);
    static intptr_t GetModuleBaseAddress();
    static std::string GetScriptPath(const std::string& scriptUID);
    static jenova::FunctionList GetFunctionsList(const std::string& scriptUID);
    static jenova::FunctionAddress GetFunctionAddress(const std::string& functionName, const std::string& scriptUID);
    static jenova::ParameterTypeList GetFunctionParameters(const std::string& functionName, const std::string& scriptUID);
    static std::string GetFunctionReturn(const std::string& functionName, const std::string& scriptUID);
    static uintptr_t GetResolvedParameterPointer(const godot::Object* objectPtr, const godot::Variant* functionParameter, const std::string& parameterType);
    static bool IsFunctionReturnable(const std::string& returnType);
    static jenova::ScriptFunctionContainer CreateFunctionContainer(const std::string& scriptUID);
    static jenova::ScriptPropertyContainer CreatePropertyContainer(const std::string& scriptUID);
    static jenova::ScriptFunctionContainer GetFunctionContainer(const std::string& scriptUID);
    static jenova::ScriptPropertyContainer GetPropertyContainer(const std::string& scriptUID);
    static Variant CallFunction(const godot::Object* objectPtr, void* instance, const std::string& functionName, std::string& scriptUID, const Variant** functionParameters, const int functionParametersCount);
    static void* ResolveFunctionHandle(const std::string& functionName, const std::string& scriptUID);
    static Variant CallFunctionByHandle(void* functionHandle, const godot::Object* objectPtr, void* instance, const Variant** functionParameters, const int functionParametersCount);
    static void CallFunctionByHandleInto(void* functionHandle, const godot::Object* objectPtr, void* instance, const Variant** functionParameters, const int functionParametersCount, Variant& r_return);
    static void SetExecutionPermission(bool executionState);
    // Inline: both are read or written twice per script call and were cross-unit calls.
    static void SetExecutionState(bool executionState) { isExecuting = executionState; }
    static bool IsExecutingFunction() { return isExecuting; }
    static void AbortExecution();
    static std::string GenerateFunctionUniqueID(const std::string& scriptPath, const std::string& functionName);
    static Variant GenerateFunctionCallError(const std::string& functionName, const String& errorReason);
    static bool FlushPropertyStorage();
    static jenova::PropertyList GetPropertiesList(std::string& scriptUID);
    static std::string GetPropertyType(const std::string& propertyName, std::string& scriptUID);
    static jenova::PropertyAddress GetPropertyAddress(const std::string& propertyName, std::string& scriptUID);
    static jenova::PropertyPointer GetPropertyPointer(const String& propertyName, const String& scriptUID);
    static bool SetPropertyValueFromVariant(const String& propertyName, const Variant& propertyValue, const String& scriptUID);
    static jenova::InterpreterBackend GetInterpreterBackend();
    static void SetInterpreterBackend(jenova::InterpreterBackend newBackend);
    static jenova::FunctionPointer SolveVirtualFunction(jenova::ModuleHandle moduleHandle, const char* functionName);
    static void SetDebugModeExecutionState(bool debugModeState);
    static bool GetDebugModeExecutionState();
    static jenova::ModuleHandle LoadShellModule(const uint8_t* moduleDataPtr, size_t moduleSize);

public:
    // Metadata Management API
    static jenova::SerializedData GenerateModuleMetadata(const std::string& mapFilePath, const jenova::ModuleList& scriptModules, const jenova::BuildResult& buildResult);
    static void GenerateExtraMetadata(jenova::json_t& metaData, const jenova::BuildResult& buildResult);
    static bool UpdateConfigurationsFromMetaData(const jenova::SerializedData& metaData);
    static bool UpdatePropertyStorageFromMetaData();
    static bool BuildMetadataCache();

public:
    // Module Database API
    static bool CreateModuleDatabase(const std::string& moduleDatabaseName, const uint8_t* moduleDataPtr, size_t moduleSize, const jenova::SerializedData& metaData);
    static bool CreateModuleDatabase(const std::string& moduleDatabaseName, const jenova::BuildResult& buildResult);
    static bool DeployFromDatabase(const std::string& moduleDatabaseName);
    static bool IsDatabaseAvailable(const std::string& moduleDatabaseName);

private:
    static inline bool                          isInitialized           = false;
    static inline bool                          allowExecution          = false;
    static inline bool                          isExecuting             = false;
    static inline bool                          isCacheReady            = false;
    static inline jenova::ModuleHandle          moduleHandle            = nullptr;
    static inline jenova::ModuleAddress         moduleBaseAddress       = 0;
    static inline jenova::json_t                moduleMetaData          = "{}";
    static inline jenova::MetadataCache         metadataCache           = jenova::MetadataCache();
    static inline size_t                        moduleBinarySize        = 0;
    static inline bool                          hasDebugInformation     = false;
    static inline bool                          executeInDebugMode      = false;
    static inline std::string                   moduleDiskPath          = "";
    static inline jenova::InterpreterBackend    interpreterBackend      = jenova::InterpreterBackend::TinyCC;
    static inline jenova::PointerStorage        propertyStorage         = jenova::PointerStorage();
    static inline jenova::PropertySetMethod     propertySetMethod       = jenova::PropertySetMethod::DirectAssign;

    /*
        Bumped on every module load/unload. Function addresses, the property storage
        pointers and the module-relative property addresses all die with the module, so
        anything caching them (script instances) compares this to know it must rebuild.
    */
    static inline uint64_t                      moduleGeneration        = 0;
};