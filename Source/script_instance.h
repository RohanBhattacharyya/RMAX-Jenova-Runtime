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

// Jenova Script Instance Definition
class CPPScriptInstance : public jenova::ScriptInstanceExtension
{
protected:
	Object* owner = nullptr;
	Ref<CPPScript> script;
	Dictionary instanceProperties;
	size_t refCount = 0;
	String scriptInstanceIdentity = "";
	bool isDeleting = false;

	mutable List<MethodInfo> methodsInfo;

private:
	void update_methods() const;

private:
	/*
		Per-call fast path.

		callp() used to, on every single engine callback, fetch the script's function list
		by value, build a StringName per entry to compare against, convert the script UID
		and the method name to std::string, and push every property through a lookup that
		concatenated Godot Strings to form its key. At 60 Hz per script instance that
		dominated the cost of calling into C++.

		None of it changes while the module is loaded, so it is resolved once and keyed on
		the interpreter's module generation, which bumps on every load/unload.
	*/
	struct PropertyBinding
	{
		StringName				name;
		std::string				typeName;
		jenova::PropertyAddress	address = 0;		// module global holding the property pointer
		jenova::PropertyPointer	storage = nullptr;	// storage owned by THIS instance
		Variant*				slot = nullptr;		// mirror inside instanceProperties, for get()/serialization
	};
	mutable uint64_t							callCacheGeneration = 0;
	mutable bool								callCacheValid = false;
	mutable bool								propertyMirrorIsStale = false;
	mutable std::string							cachedIdentity;
	/*
		Method lookup.

		godot-cpp's StringName comparison and hashing both cross back into the engine, so
		matching a method name that way cost several engine calls per script call. A
		StringName is an interned pointer, so its raw bits identify the name: a linear scan
		of 8-byte keys over the handful of methods a script has needs no engine call at all.
	*/
	struct MethodBinding
	{
		uint64_t	nameKey = 0;			// StringName's interned pointer bits
		void*		handle  = nullptr;		// interpreter function handle
	};
	static uint64_t MethodNameKey(const StringName& methodName)
	{
		// Exactly one StringName wide: it is pointer sized, which is 4 bytes on wasm32 and
		// 8 on a 64-bit target. Reading a fixed 8 would pull in neighbouring memory and no
		// two keys would ever match.
		static_assert(sizeof(StringName) <= sizeof(uint64_t), "StringName no longer fits a key.");
		uint64_t nameKey = 0;
		memcpy(&nameKey, &methodName, sizeof(StringName));
		return nameKey;
	}
	void* FindMethodHandle(const StringName& methodName) const
	{
		const uint64_t nameKey = MethodNameKey(methodName);
		for (const MethodBinding& binding : cachedMethods) if (binding.nameKey == nameKey) return binding.handle;
		return nullptr;
	}
	mutable std::vector<MethodBinding>			cachedMethods;
	mutable std::vector<StringName>				cachedMethodNames;
	mutable std::vector<PropertyBinding>		cachedProperties;

	// Checked on every script call, so the hit path is inline and the rebuild is not.
	void EnsureCallCache() const
	{
		if (callCacheValid && callCacheGeneration == JenovaInterpreter::GetModuleGeneration()) return;
		RebuildCallCache();
	}
	void RebuildCallCache() const;
	void SavePropertyPointers(std::vector<void*>& savedPointers) const;
	void RestorePropertyPointers(const std::vector<void*>& savedPointers) const;
	void CallInternalMethod(const StringName& p_method, GDExtensionCallError& r_error, Variant& r_return);
	void ReleaseCachedProperties() const;
	void SyncPropertyMirror() const;

public:
	// Base Methods
	bool set(const StringName& p_name, const Variant& p_value) override;
	bool get(const StringName& p_name, Variant& r_ret) const override;
	const GDExtensionPropertyInfo* get_property_list(uint32_t* r_count) const override;
	void free_property_list(const GDExtensionPropertyInfo* p_list, uint32_t p_count) const override;
	Variant::Type get_property_type(const StringName& p_name, bool* r_is_valid) const override;
	bool validate_property(GDExtensionPropertyInfo& p_property) const override;
	bool property_can_revert(const StringName& p_name) const override;
	bool property_get_revert(const StringName& p_name, Variant& r_ret) const override;
	Object* get_owner() override;
	void get_property_state(GDExtensionScriptInstancePropertyStateAdd p_add_func, void* p_userdata) override;
	const GDExtensionMethodInfo* get_method_list(uint32_t* r_count) const override;
	void free_method_list(const GDExtensionMethodInfo* p_list, uint32_t p_count) const override;
	bool has_method(const StringName& p_method) const override;
	int get_method_argument_count(const StringName& p_method, bool* r_is_valid = nullptr) const override;
	Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) override;
	void callp_into(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error, Variant& r_return) override;
	void notification(int p_notification, bool p_reversed) override;
	String to_string(bool* r_valid) override;
	void refcount_incremented() override;
	bool refcount_decremented() override;
	Ref<Script> get_script() const override;
	bool is_placeholder() const override;
	void property_set_fallback(const StringName& p_name, const Variant& p_value, bool* r_valid) override;
	Variant property_get_fallback(const StringName& p_name, bool* r_valid) override;
	ScriptLanguage* _get_language() override;

public:
	// Memory Storage
	mutable std::vector<std::vector<GDExtensionPropertyInfo>*> methodInfoPointers;

public:
	// Methods
	String GetIdentity() const;
	bool ForcePushProperties();
	bool ForcePullProperties();

	// Initializer/Destructor
	CPPScriptInstance(Object* p_owner, const Ref<CPPScript> p_script);
	~CPPScriptInstance();
};
