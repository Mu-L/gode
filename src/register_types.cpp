#include "register_types.h"
#include "utils/node_runtime.h"

#include "napi.h"
#include "support/javascript/javascript.h"
#include "support/javascript/javascript_language.h"
#include "support/javascript/javascript_loader.h"
#include "support/javascript/javascript_saver.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_node_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(gode::Javascript);
	GDREGISTER_CLASS(gode::JavascriptLanguage);
	GDREGISTER_CLASS(gode::JavascriptSaver);
	GDREGISTER_CLASS(gode::JavascriptLoader);
	Engine::get_singleton()->register_script_language(gode::JavascriptLanguage::get_singleton());
	ResourceSaver::get_singleton()->add_resource_format_saver(gode::JavascriptSaver::get_singleton());
	ResourceLoader::get_singleton()->add_resource_format_loader(gode::JavascriptLoader::get_singleton());

	gode::NodeRuntime::run_script("GD.print('Hello World!');");

	Napi::HandleScope scope(gode::JsEnvManager::get_env());

	// Test compilation and default export
	Napi::Value exports = gode::NodeRuntime::compile_script(
			"class MyClass {"
			"  constructor() { GD.print('MyClass constructed'); }"
			"  myMethod() { GD.print('MyClass method called'); }"
			"}"
			"module.exports = { default: MyClass };",
			"test.js");

	Napi::Function default_class = gode::NodeRuntime::get_default_class(exports);
	if (!default_class.IsEmpty()) {
		Napi::Object instance = default_class.New({});
		Napi::Function method = instance.Get("myMethod").As<Napi::Function>();
		method.Call(instance, {});
	}
}

void uninitialize_node_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	Engine::get_singleton()->unregister_script_language(gode::JavascriptLanguage::get_singleton());
	ResourceSaver::get_singleton()->remove_resource_format_saver(gode::JavascriptSaver::get_singleton());
	ResourceLoader::get_singleton()->remove_resource_format_loader(gode::JavascriptLoader::get_singleton());

	gode::NodeRuntime::shutdown();
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT node_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_node_module);
	init_obj.register_terminator(uninitialize_node_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
