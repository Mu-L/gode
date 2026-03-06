#include "support/javascript/javascript_callable.h"

#include "utils/node_runtime.h"
#include "utils/value_convert.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <v8-isolate.h>
#include <v8-locker.h>

namespace gode {

JavascriptCallable::JavascriptCallable(Napi::Function p_function) {
	func_ref = Napi::Persistent(p_function);
}

JavascriptCallable::~JavascriptCallable() {
	if (!func_ref.IsEmpty()) {
		func_ref.Reset();
	}
}

Napi::Function JavascriptCallable::get_function() const {
	if (func_ref.IsEmpty()) {
		return Napi::Function();
	}
	return func_ref.Value();
}

uint32_t JavascriptCallable::hash() const {
	return (uint64_t)this;
}

godot::String JavascriptCallable::get_as_text() const {
	return "JavascriptCallable";
}

bool JavascriptCallable::is_valid() const {
    // godot::UtilityFunctions::print("DEBUG: JavascriptCallable::is_valid invoked"); // Too spammy?
	return !func_ref.IsEmpty();
}

godot::ObjectID JavascriptCallable::get_object() const {
	return godot::ObjectID();
}

void JavascriptCallable::call(const godot::Variant **p_arguments, int p_argcount, godot::Variant &r_return_value, GDExtensionCallError &r_call_error) const {
	v8::Locker locker(NodeRuntime::isolate);
	v8::Isolate::Scope isolate_scope(NodeRuntime::isolate);
	Napi::HandleScope scope(JsEnvManager::get_env());

	if (func_ref.IsEmpty()) {
		r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
		return;
	}

	Napi::Function func = func_ref.Value();
	std::vector<napi_value> args;
	for (int i = 0; i < p_argcount; ++i) {
		args.push_back(godot_to_napi(JsEnvManager::get_env(), *p_arguments[i]));
	}

	try {
		Napi::Value result = func.Call(JsEnvManager::get_env().Global(), args);
		r_return_value = napi_to_godot(result);
		r_call_error.error = GDEXTENSION_CALL_OK;
	} catch (const Napi::Error &e) {
		godot::UtilityFunctions::printerr("JS Exception in Callable: ", e.Message().c_str());
		r_call_error.error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	}
}

static bool javascript_callable_equal_func(const godot::CallableCustom *p_a, const godot::CallableCustom *p_b) {
	const JavascriptCallable *a = static_cast<const JavascriptCallable *>(p_a);
	const JavascriptCallable *b = static_cast<const JavascriptCallable *>(p_b);
    v8::Locker locker(NodeRuntime::isolate);
	v8::Isolate::Scope isolate_scope(NodeRuntime::isolate);
	Napi::HandleScope scope(JsEnvManager::get_env());
	return p_a == p_b;
}

static bool javascript_callable_less_than_func(const godot::CallableCustom *p_a, const godot::CallableCustom *p_b) {
	return p_a < p_b;
}

godot::CallableCustom::CompareEqualFunc JavascriptCallable::get_compare_equal_func() const {
	return javascript_callable_equal_func;
}

godot::CallableCustom::CompareLessFunc JavascriptCallable::get_compare_less_func() const {
	return javascript_callable_less_than_func;
}

} // namespace gode
