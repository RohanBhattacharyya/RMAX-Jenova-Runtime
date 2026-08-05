
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

// Helper Functions
static String* AllocateString(const String& p_str) 
{
	String* ptr = memnew(String);
	*ptr = p_str;
	return ptr;
}
static StringName* AllocateStringName(const String& p_str) 
{
	StringName* ptr = memnew(StringName);
	*ptr = p_str;
	return ptr;
}
static GDExtensionPropertyInfo CreatePropertyType(const Dictionary& p_src) 
{
	GDExtensionPropertyInfo p_dst;
	p_dst.type = GDExtensionVariantType::GDEXTENSION_VARIANT_TYPE_NIL;
	p_dst.name = AllocateStringName(p_src["name"]);
	p_dst.class_name = AllocateStringName(p_src["class_name"]);
	p_dst.hint = p_src["hint"];
	p_dst.hint_string = AllocateString(p_src["hint_string"]);
	p_dst.usage = p_src["usage"];
	return p_dst;
}
static void AddState(GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value, void* p_userdata) 
{
	List<Pair<StringName, Variant>>* list = reinterpret_cast<List<Pair<StringName, Variant>>*>(p_userdata);
	list->push_back({ *(const StringName*)p_name, *(const Variant*)p_value });
}
static GDExtensionMethodInfo CreateMethodInfo(const MethodInfo& methodInfo, CPPScriptInstance* instance)
{
	// Create Extension Parameters Info
	std::vector<GDExtensionPropertyInfo>* parametersInfo = new std::vector<GDExtensionPropertyInfo>();
	instance->methodInfoPointers.push_back(parametersInfo);

	for (const auto& parameter : methodInfo.arguments)
	{
		parametersInfo->push_back(GDExtensionPropertyInfo
		{
			GDExtensionVariantType(parameter.type),
			AllocateStringName(parameter.name),
			AllocateStringName(parameter.class_name),
			parameter.hint,
			AllocateString(parameter.hint_string),
			parameter.usage
		});
	}

	// Create Extension Method Info
	return GDExtensionMethodInfo
	{
		AllocateStringName(methodInfo.name),
		GDExtensionPropertyInfo
		{
			GDExtensionVariantType(methodInfo.return_val.type),
			AllocateStringName(methodInfo.return_val.name),
			AllocateStringName(methodInfo.return_val.class_name),
			methodInfo.return_val.hint,
			AllocateString(methodInfo.return_val.hint_string),
			methodInfo.return_val.usage
		},
		methodInfo.flags,
		methodInfo.id,
		(uint32_t)methodInfo.arguments.size(),
		parametersInfo->data(),
		0, nullptr // Default Arguments Not Supported Yet
	};
}
template<typename T> T* memnew_with_size(int p_size)
{
	uint64_t size = sizeof(T) * p_size;
	void* ptr = memalloc(size + sizeof(int));
	*((int*)ptr) = p_size;
	return (T*)((int*)ptr + 1);
}
template<typename T> void memdelete_with_size(const T* p_ptr)
{
	memfree((int*)p_ptr - 1);
}
template<typename T> int memnew_ptr_size(const T* p_ptr)
{
	return !p_ptr ? 0 : *((int*)p_ptr - 1);
}
static void FreePropertyList(const GDExtensionPropertyInfo& p_property)
{
	memdelete((StringName*)p_property.name);
	memdelete((StringName*)p_property.class_name);
	memdelete((String*)p_property.hint_string);
}

// C++ Script Instance Implementation
bool CPPScriptInstance::set(const StringName& p_name, const Variant& p_value)
{
	// Remove
	jenova::VerboseByID(__LINE__, "Setting Property (%s)...", AS_C_STRING(p_name));

	// Set Embedded Source [Internal]
	if (p_name == StringName("cpp/source_code") && script.is_valid() && script->is_built_in())
	{
		script->set_source_code(jenova::RetriveStringFromSecuredBase64String(p_value));
		return true;
	}

	// Set Interpreted Properties. Written straight into this instance's storage, which is
	// what the script reads, and mirrored so serialization sees it too.
	EnsureCallCache();
	for (const PropertyBinding& binding : cachedProperties)
	{
		if (binding.name != p_name) continue;
		*binding.slot = p_value;
		jenova::SetPropertyPointerValueFromVariant(binding.storage, p_value);
		return true;
	}

	// Not Handled
	return false;
}
bool CPPScriptInstance::get(const StringName& p_name, Variant& r_ret) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "Getting Property (%s)...", AS_C_STRING(p_name));

	// Get Script
	if (p_name == StringName("script"))
	{
		r_ret = script;
		return true;
	}

	// Get Embedded Source [Internal]
	if (p_name == StringName("cpp/source_code") && script.is_valid() && script->is_built_in())
	{
		r_ret = jenova::CreateSecuredBase64StringFromString(script->get_source_code());
		return true;
	}

	// Get Interpreted Properties
	EnsureCallCache();
	SyncPropertyMirror();
	if (instanceProperties.has(p_name))
	{
		r_ret = instanceProperties[p_name];
		return true;
	}

	// Not Handled
	return false;
}
godot::String CPPScriptInstance::to_string(bool* r_is_valid)
{
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::to_string");
	*r_is_valid = true;
	return String(jenova::Format("<JenovaScript:%s>", AS_C_STRING(GetIdentity())).c_str());
}
void CPPScriptInstance::notification(int p_notification, bool p_reversed)
{
	if (p_notification == Object::NOTIFICATION_PREDELETE) isDeleting = true;
}
Variant CPPScriptInstance::callp(const StringName& p_method, const Variant** p_args, const int p_argument_count, GDExtensionCallError& r_error)
{
	Variant callResult;
	callp_into(p_method, p_args, p_argument_count, r_error, callResult);
	return callResult;
}
void CPPScriptInstance::callp_into(const StringName& p_method, const Variant** p_args, const int p_argument_count, GDExtensionCallError& r_error, Variant& r_return)
{
	// Validate Scripting Backend
	if (!jenova::GlobalSettings::ScriptingEnabled)
	{
		r_error.error = GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	// Validate Instance & Script
	if (isDeleting || !this->script.is_valid())
	{
		r_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
		return;
	}

	// Verbose Call
	if (jenova::GlobalStorage::DeveloperModeActivated)
	{
		String script_name = this->script->get_path().get_file();
		String owner_name = godot::Object::cast_to<godot::Node>(this->owner)->get_name();
		jenova::VerboseByID(__LINE__, "Executing Script (%s | %s)[%s][%d] from (%s | %p) ...",
			AS_C_STRING(script_name), AS_C_STRING(GetIdentity()), AS_C_STRING(p_method), p_argument_count, AS_C_STRING(owner_name), this->instance);
	}

	// Resolve Everything That Is Fixed For The Lifetime Of The Loaded Module.
	EnsureCallCache();

	/*
		Call to Interpreter.

		Script methods are matched first. The internal method names below are never script
		methods, and comparing them costs an engine round trip each in godot-cpp, so they
		are only reached when the name is not the script's.
	*/
	void* jenovaMethod = FindMethodHandle(p_method);
	if (jenovaMethod)
	{
		// Abort Call In Editor If Script is Not Tool
		if (QUERY_ENGINE_MODE(Editor) && !script->is_tool())
		{
			r_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
			return;
		}

		/*
			Point The Module's Property Globals At This Instance.

			A nested call -- this instance being called from inside another script's call,
			which is what two objects of the same class talking to each other looks like --
			has to do the same, and then put the outer instance's pointers back. Skipping the
			push left the inner script reading and writing the outer instance's properties.

			Only the nested case pays for saving them; the common top-level call is one store
			per property, as before.
		*/
		const bool nestedCall = JenovaInterpreter::IsExecutingFunction();
		std::vector<void*> outerPropertyPointers;
		if (!cachedProperties.empty())
		{
			if (nestedCall) SavePropertyPointers(outerPropertyPointers);
			ForcePushProperties();
		}

		// Invoke Function & Call. The result goes straight into the Variant the engine
		// supplied; a void script function leaves it untouched, which is the NIL the engine
		// already put there.
		JenovaInterpreter::CallFunctionByHandleInto(jenovaMethod, this->owner, this, p_args, p_argument_count, r_return);

		// Hand the module's property globals back to the instance we interrupted.
		if (nestedCall && !cachedProperties.empty()) RestorePropertyPointers(outerPropertyPointers);

		// The script wrote into this instance's own storage, so the instanceProperties
		// mirror is only marked stale here and refreshed when something reads it.
		if (jenova::GlobalSettings::UpdatePropertiesAfterCall && !cachedProperties.empty()) propertyMirrorIsStale = true;

		// Return Result
		r_error.error = GDEXTENSION_CALL_OK;
		return;
	}

	// Not a script method: the editor's internal ones, out of line so the hot call path
	// above keeps a small frame.
	CallInternalMethod(p_method, r_error, r_return);
}
void CPPScriptInstance::CallInternalMethod(const StringName& p_method, GDExtensionCallError& r_error, Variant& r_return)
{
	// Handle Internal Methods. Constructed once, since building a StringName hashes the
	// text and takes the global name table lock.
	static const StringName editorNameMethod("_get_editor_name");
	static const StringName hideFromInspectorMethod("_hide_script_from_inspector");
	static const StringName readOnlyMethod("_is_read_only");
	if (p_method == editorNameMethod)
	{
		r_error.error = GDEXTENSION_CALL_OK;
		r_return = Variant(String(jenova::Format("[ %s � Powered by Jenova ]", AS_C_STRING(godot::Object::cast_to<godot::Node>(this->owner)->get_name())).c_str()));
		return;
	}
	else if (p_method == hideFromInspectorMethod)
	{
		r_error.error = GDEXTENSION_CALL_OK;
		r_return = false;
		return;
	}
	else if (p_method == readOnlyMethod)
	{
		r_error.error = GDEXTENSION_CALL_OK;
		r_return = false;
		return;
	}

	// Default Result
	r_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
}
void CPPScriptInstance::update_methods() const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::update_methods (%s)", AS_C_STRING(GetIdentity()));

	// Validate Script
	if (script.is_null()) return;

	// Update Script Methods
	methodsInfo.clear();
	auto functionContainer = JenovaInterpreter::GetFunctionContainer(AS_STD_STRING(GetIdentity()));
	for (auto& scriptFunction : functionContainer.scriptFunctions)
	{
		if (scriptFunction.functionName.contains(StringName("__jnvsignal__"))) continue;
		this->methodsInfo.push_back(scriptFunction.methodInfo);
	}
}
const GDExtensionMethodInfo* CPPScriptInstance::get_method_list(uint32_t* r_count) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_method_list (%s)", AS_C_STRING(GetIdentity()));

	// Validate Script
	if (script.is_null())
	{
		*r_count = 0;
		return nullptr;
	}

	// Update Methods
	this->update_methods();

	// Create Method List
	const int size = methodsInfo.size();
	GDExtensionMethodInfo* list = memnew_arr(GDExtensionMethodInfo, size);
	int i = 0;
	for (auto& methodInfo : methodsInfo)
	{
		CPPScriptInstance* self = const_cast<CPPScriptInstance*>(this);
		list[i] = CreateMethodInfo(methodInfo, self);
		i++;
	}

	// Remove
	jenova::VerboseByID(__LINE__, "Method List returned %d functions", size);

	// Return Methods
	*r_count = size;
	return list;
}
void CPPScriptInstance::free_method_list(const GDExtensionMethodInfo* p_list, uint32_t p_count) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::free_method_list");

	if (p_list) memdelete_arr(p_list);
}
const GDExtensionPropertyInfo* CPPScriptInstance::get_property_list(uint32_t* r_count) const
{
	// Create Property List
	LocalVector<GDExtensionPropertyInfo> propertiesInfo;

	// Add Properties
	if (script.is_valid() && script->is_built_in())
	{
		GDExtensionPropertyInfo sourceCodeProperty = {};
		sourceCodeProperty.type = GDEXTENSION_VARIANT_TYPE_STRING;
		sourceCodeProperty.name = AllocateStringName("cpp/source_code");
		sourceCodeProperty.class_name = AllocateStringName("Variant");
		sourceCodeProperty.hint_string = AllocateString("");
		sourceCodeProperty.hint = PROPERTY_HINT_NONE;
		sourceCodeProperty.usage = PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL;
		propertiesInfo.push_back(sourceCodeProperty);
	}

	// Add Jenova Script Interpreted Properties
	auto propContainer = JenovaInterpreter::GetPropertyContainer(AS_STD_STRING(GetIdentity()));
	for (size_t i = 0; i < propContainer.scriptProperties.size(); i++)
	{
		GDExtensionPropertyInfo sourceCodeProperty = {};
		sourceCodeProperty.type = GDExtensionVariantType(propContainer.scriptProperties[i].propertyInfo.type);
		sourceCodeProperty.name = AllocateStringName(propContainer.scriptProperties[i].propertyInfo.name);
		sourceCodeProperty.class_name = AllocateStringName(propContainer.scriptProperties[i].propertyInfo.class_name);
		sourceCodeProperty.hint_string = AllocateString(propContainer.scriptProperties[i].propertyInfo.hint_string);
		sourceCodeProperty.hint = propContainer.scriptProperties[i].propertyInfo.hint;
		sourceCodeProperty.usage = propContainer.scriptProperties[i].propertyInfo.usage;
		propertiesInfo.push_back(sourceCodeProperty);
	}

	// Add Jenova Script User-Defined Properties

	// Set Properties Size
	*r_count = propertiesInfo.size();
	if (propertiesInfo.size() == 0) return nullptr;

	// Create Property Final List
	GDExtensionPropertyInfo* list = memnew_with_size<GDExtensionPropertyInfo>(propertiesInfo.size());
	memcpy(list, propertiesInfo.ptr(), sizeof(GDExtensionPropertyInfo) * propertiesInfo.size());
	return list;
}
void CPPScriptInstance::free_property_list(const GDExtensionPropertyInfo* p_list, uint32_t p_count) const
{
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::free_property_list");
	if (p_list)
	{
		int size = memnew_ptr_size<GDExtensionPropertyInfo>(p_list);
		for (int i = 0; i < size; i++) FreePropertyList(p_list[i]);
		memdelete_with_size<GDExtensionPropertyInfo>(p_list);
	}
}
Variant::Type CPPScriptInstance::get_property_type(const StringName& p_name, bool* r_is_valid) const
{
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_property_type");

	// Handle Internal Properties
	if (p_name == StringName("cpp/source_code"))
	{
		if (r_is_valid) *r_is_valid = true;
		return Variant::STRING;
	}

	// Handle Interpreted Properties
	auto propContainer = JenovaInterpreter::GetPropertyContainer(AS_STD_STRING(GetIdentity()));
	for (size_t i = 0; i < propContainer.scriptProperties.size(); i++)
	{
		if (p_name == propContainer.scriptProperties[i].propertyInfo.name)
		{
			if (r_is_valid) *r_is_valid = true;
			return propContainer.scriptProperties[i].propertyInfo.type;
		}
	}

	// Not Found
	*r_is_valid = false;
	return Variant::NIL;
}
void CPPScriptInstance::get_property_state(GDExtensionScriptInstancePropertyStateAdd p_add_func, void* p_userdata)
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_property_state");

	p_add_func = AddState; // Needs Investigation
}
bool CPPScriptInstance::validate_property(GDExtensionPropertyInfo& p_property) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::validate_property %s", AS_C_STRING(*(StringName*)p_property.name));

	// Verify & Get Property Name
	if (!p_property.name) return false;
	StringName propertyName = *(StringName*)p_property.name;

	// Handle Interpreted Properties
	if (instanceProperties.has(propertyName))
	{
		return true;
	}
	else
	{
		auto propContainer = JenovaInterpreter::GetPropertyContainer(AS_STD_STRING(GetIdentity()));
		for (size_t i = 0; i < propContainer.scriptProperties.size(); i++)
		{
			if (propertyName == propContainer.scriptProperties[i].propertyInfo.name) return true;
		}
	}

	// Not Implemented Yet
	return false;
}
bool CPPScriptInstance::has_method(const StringName& p_name) const
{
	// Remove. Guarded, the arguments alone cost two heap allocations each.
	if (jenova::GlobalStorage::DeveloperModeActivated)
	{
		jenova::VerboseByID(__LINE__, "CPPScriptInstance::has_method (%s) [%s]", AS_C_STRING(GetIdentity()), AS_C_STRING(p_name));
	}

	// Validate Script
	if (!script.is_valid()) return false;
	if (script.is_null()) return false;
	bool result = false;

	// Search Over Pre-Defined Functions [These will be not filtered by Tool Mode]
	static const StringName godotFunctionNames[] =
	{
		StringName("_get_editor_name"),
		StringName("_hide_script_from_inspector"),
		StringName("_is_read_only"),
	};
	for (const StringName& function : godotFunctionNames)
	{
		if (p_name == function)
		{
			result = true;
			break;
		}
	}

	// Jenova Module Functions Handling
	if (!result)
	{
		// Search Over User Defined Functions
		EnsureCallCache();
		result = FindMethodHandle(p_name) != nullptr;

		// In Editor and Tool Mode We Return All Functions As True
		if (!result && QUERY_ENGINE_MODE(Editor) && script->is_tool()) result = true;
	}

	// Remove
	if (jenova::GlobalStorage::DeveloperModeActivated)
	{
		jenova::VerboseByID(__LINE__, "CPPScriptInstance::has_method (%s) [%s] returned %s", AS_C_STRING(GetIdentity()), AS_C_STRING(p_name), result ? "TRUE" : "FALSE");
	}
	return result;
}
int CPPScriptInstance::get_method_argument_count(const StringName& p_method, bool* r_is_valid) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_method_argument_count");
	*r_is_valid = false;
	return 0;
}
bool CPPScriptInstance::property_can_revert(const StringName& p_name) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::property_can_revert");

	// Handle Interpreted Properties
	auto propContainer = JenovaInterpreter::GetPropertyContainer(AS_STD_STRING(GetIdentity()));
	for (size_t i = 0; i < propContainer.scriptProperties.size(); i++)
	{
		if (p_name == propContainer.scriptProperties[i].propertyInfo.name) return true;
	}

	// Not Found
	return false;
}
bool CPPScriptInstance::property_get_revert(const StringName& p_name, Variant& r_ret) const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::property_get_revert");

	// Handle Interpreted Properties
	auto propContainer = JenovaInterpreter::GetPropertyContainer(AS_STD_STRING(GetIdentity()));
	for (size_t i = 0; i < propContainer.scriptProperties.size(); i++)
	{
		if (p_name == propContainer.scriptProperties[i].propertyInfo.name)
		{
			r_ret = propContainer.scriptProperties[i].defaultValue;
			return true;
		}
	}

	// Not Found
	return false;
}
void CPPScriptInstance::refcount_incremented()
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::refcount_incremented");

	refCount++;
}
bool CPPScriptInstance::refcount_decremented()
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::refcount_decremented");

	refCount--;
	return false;
}
Object* CPPScriptInstance::get_owner()
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_owner");

	return owner;
}
Ref<Script> CPPScriptInstance::get_script() const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::get_script");
	return script;
}
bool CPPScriptInstance::is_placeholder() const
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::is_placeholder");

	return false;
}
void CPPScriptInstance::property_set_fallback(const StringName& p_name, const Variant& p_value, bool* r_valid)
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::property_set_fallback");

	*r_valid = false;
}
Variant CPPScriptInstance::property_get_fallback(const StringName& p_name, bool* r_valid)
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::property_get_fallback");

	*r_valid = false;
	return Variant::NIL;
}
ScriptLanguage* CPPScriptInstance::_get_language()
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::_get_language");

	return CPPScriptLanguage::get_singleton();
}
String CPPScriptInstance::GetIdentity() const
{
	return scriptInstanceIdentity;
}
void CPPScriptInstance::ReleaseCachedProperties() const
{
	// If the module is still loaded, aim its property globals back at the interpreter's
	// own storage before this instance's storage goes away. A module shutdown event that
	// touches a property after the last instance is gone would otherwise read freed memory.
	const bool moduleStillLoaded = callCacheGeneration == JenovaInterpreter::GetModuleGeneration();
	const String scriptIdentity = GetIdentity();
	for (const PropertyBinding& binding : cachedProperties)
	{
		if (moduleStillLoaded && binding.address)
		{
			jenova::PropertyPointer sharedStorage = JenovaInterpreter::GetPropertyPointer(String(binding.name), scriptIdentity);
			if (sharedStorage)
			{
				if (JenovaInterpreter::GetPropertySetMethod() == jenova::PropertySetMethod::MemoryCopy) memcpy((void*)binding.address, &sharedStorage, sizeof(sharedStorage));
				else *(void**)binding.address = sharedStorage;
			}
		}
		jenova::FreeVariantBasedProperty(binding.storage, binding.typeName);
	}
	cachedProperties.clear();
}
void CPPScriptInstance::SyncPropertyMirror() const
{
	// The script writes straight into this instance's storage, so instanceProperties is
	// only brought up to date when something actually reads it.
	if (!propertyMirrorIsStale) return;
	propertyMirrorIsStale = false;
	for (const PropertyBinding& binding : cachedProperties)
	{
		jenova::GetVariantFromPropertyPointer(binding.storage, *binding.slot, binding.slot->get_type(), binding.typeName);
	}
}
void CPPScriptInstance::RebuildCallCache() const
{
	callCacheGeneration = JenovaInterpreter::GetModuleGeneration();
	callCacheValid = true;
	propertyMirrorIsStale = false;
	cachedIdentity = AS_STD_STRING(GetIdentity());
	cachedMethods.clear();
	cachedMethodNames.clear();
	ReleaseCachedProperties();

	// Script Methods. Keyed by StringName so callp() is one hash lookup, and the value is
	// the interpreter's resolved function handle, so the call skips the interpreter's own
	// mutex and two string-keyed lookups as well.
	for (const std::string& functionName : JenovaInterpreter::GetFunctionsList(cachedIdentity))
	{
		void* functionHandle = JenovaInterpreter::ResolveFunctionHandle(functionName, cachedIdentity);
		if (!functionHandle) continue;

		// The StringName is kept alive by cachedMethodNames so its interned pointer, which
		// is what the key is, cannot be recycled while the binding is in use.
		cachedMethodNames.push_back(StringName(functionName.c_str()));
		cachedMethods.push_back({ MethodNameKey(cachedMethodNames.back()), functionHandle });
	}

	/*
		Script Properties.

		A script's properties are plain globals in the module, reached through a pointer
		global that Jenova sets. That pointer used to be aimed at one storage slot shared by
		every instance of the script, so entering a call meant copying this instance's values
		into the shared slot and leaving it meant copying them back out. Two Variant
		assignments per property per call, and it was the single most expensive thing about
		calling into C++.

		Every instance owns its storage now. Entering a call just aims the module's pointer
		at this instance, which is one store, and the script then reads and writes the
		instance's own memory, so there is nothing to copy back.
	*/
	const String scriptIdentity = GetIdentity();
	jenova::ScriptPropertyContainer propertyContainer = JenovaInterpreter::GetPropertyContainer(cachedIdentity);
	for (const jenova::ScriptProperty& scriptProperty : propertyContainer.scriptProperties)
	{
		PropertyBinding binding;
		binding.name = scriptProperty.propertyInfo.name;

		std::string shortName = AS_STD_STRING(String(binding.name).get_file());
		binding.address = JenovaInterpreter::GetPropertyAddress(shortName, cachedIdentity);
		if (!binding.address) continue;

		binding.typeName = JenovaInterpreter::GetPropertyType(shortName, cachedIdentity);
		binding.storage = jenova::AllocateVariantBasedProperty(binding.typeName);
		if (!binding.storage) continue;

		// Address of this instance's value inside instanceProperties. Indexing creates the
		// entry if it is missing, so every script property has a slot from here on and no
		// later insert can rehash the dictionary out from under these pointers.
		// ponytail: assumes nothing erases keys from instanceProperties; nothing does.
		binding.slot = &const_cast<Dictionary&>(instanceProperties)[binding.name];
		if (binding.slot->get_type() == Variant::NIL) *binding.slot = scriptProperty.defaultValue;
		jenova::SetPropertyPointerValueFromVariant(binding.storage, *binding.slot);

		cachedProperties.push_back(binding);
	}
}
bool CPPScriptInstance::ForcePushProperties()
{
	// Aim the module's property pointers at this instance's storage. One store each.
	// callp() has already resolved the cache before reaching here.
	const bool memoryCopy = JenovaInterpreter::GetPropertySetMethod() == jenova::PropertySetMethod::MemoryCopy;
	for (const PropertyBinding& binding : cachedProperties)
	{
		if (memoryCopy) memcpy((void*)binding.address, &binding.storage, sizeof(binding.storage));
		else *(void**)binding.address = binding.storage;
	}
	propertyMirrorIsStale = true;

	// All Good
	return true;
}
void CPPScriptInstance::SavePropertyPointers(std::vector<void*>& savedPointers) const
{
	// Whatever the module's property globals currently hold, which is the storage of the
	// instance whose call this one is nested inside.
	savedPointers.clear();
	savedPointers.reserve(cachedProperties.size());
	for (const PropertyBinding& binding : cachedProperties) savedPointers.push_back(*(void**)binding.address);
}
void CPPScriptInstance::RestorePropertyPointers(const std::vector<void*>& savedPointers) const
{
	const bool memoryCopy = JenovaInterpreter::GetPropertySetMethod() == jenova::PropertySetMethod::MemoryCopy;
	for (size_t i = 0; i < cachedProperties.size() && i < savedPointers.size(); i++)
	{
		const jenova::PropertyAddress address = cachedProperties[i].address;
		void* previousPointer = savedPointers[i];
		if (memoryCopy) memcpy((void*)address, &previousPointer, sizeof(previousPointer));
		else *(void**)address = previousPointer;
	}
}
bool CPPScriptInstance::ForcePullProperties()
{
	// Nothing to pull, the script wrote into this instance's own storage. The
	// instanceProperties mirror is refreshed lazily, when something reads it.
	EnsureCallCache();
	propertyMirrorIsStale = !cachedProperties.empty();

	// All Good
	return true;
}

// C++ Script Instance Initializer/Destructor
CPPScriptInstance::CPPScriptInstance(Object* p_owner, const Ref<CPPScript> p_script) : owner(p_owner), script(p_script) 
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::CPPScriptInstance");

	// Validate Script Object
	if (p_script.is_null() || !p_script.is_valid())
	{
		jenova::VerboseByID(__LINE__, "ERROR : Null Script Passed to Instance.");
		return;
	}

	// Verbose Creation
	godot::Node* parentNode = godot::Object::cast_to<godot::Node>(p_owner);
	jenova::VerboseByID(__LINE__, "Creating Script Instance from (%s) Owner : %s", AS_C_STRING(p_script.ptr()->get_name()), AS_C_STRING(parentNode->get_name()));

	// Generate Script Identifier Hash
	scriptInstanceIdentity = jenova::GenerateStandardUIDFromPath(p_script.ptr());

	// Register Script Instance to Manager
	JenovaScriptManager::get_singleton()->add_script_instance(this);
}
CPPScriptInstance::~CPPScriptInstance() 
{
	// Remove
	jenova::VerboseByID(__LINE__, "CPPScriptInstance::~CPPScriptInstance (%s)", AS_C_STRING(this->GetIdentity()));

	// Unregister Script Instance to Manager
	JenovaScriptManager::get_singleton()->remove_script_instance(this);

	// Release This Instance's Property Storage
	ReleaseCachedProperties();

	// Release Pointers
	for (size_t i = 0; i < this->methodInfoPointers.size(); i++) delete this->methodInfoPointers[i];
	std::vector<std::vector<GDExtensionPropertyInfo>*>().swap(this->methodInfoPointers);
}