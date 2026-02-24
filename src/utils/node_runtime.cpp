#include "utils/node_runtime.h"
#include "register/utility_functions/utility_functions.h" // Added
#include <cppgc/platform.h>
#include <node.h>
#include <uv.h>
#include <memory>
#include <string>
#include <vector>

#include <node_api.h>

namespace gode {

static napi_env js_env = nullptr;
void JsEnvManager::init(Napi::Env env) {
	js_env = env;
}
Napi::Env JsEnvManager::get_env() {
	return Napi::Env(js_env);
}

static bool node_initialized = false;
static std::unique_ptr<node::MultiIsolatePlatform> platform;
static std::unique_ptr<node::ArrayBufferAllocator> allocator;
static v8::Isolate *isolate = nullptr;
static node::Environment *env = nullptr;
static node::IsolateData *isolate_data = nullptr;
static v8::Global<v8::Context> node_context;

static Napi::Object InitGodeAddon(Napi::Env env, Napi::Object exports) {
	gode::JsEnvManager::init(env);
	gode::GD::init(env, exports);
	return exports;
}

static napi_value InitGodeAddon_C(napi_env env, napi_value exports) {
	return Napi::RegisterModule(env, exports, InitGodeAddon);
}

void NodeRuntime::init_once() {
	if (node_initialized) {
		return;
	}

	std::vector<std::string> args;
	std::vector<std::string> exec_args;
	std::vector<std::string> errors;
	args.push_back("godot node");

	int flags = node::ProcessInitializationFlags::kNoInitializeV8 |
			node::ProcessInitializationFlags::kNoInitializeNodeV8Platform |
			node::ProcessInitializationFlags::kNoInitializeCppgc |
			node::ProcessInitializationFlags::kNoDefaultSignalHandling |
			node::ProcessInitializationFlags::kNoStdioInitialization;

	auto init_result = node::InitializeOncePerProcess(args, static_cast<node::ProcessInitializationFlags::Flags>(flags));

	if (!init_result->errors().empty()) {
		for (const auto &err : init_result->errors()) {
			// printf("Node init error: %s\n", err.c_str());
		}
	}

	allocator = node::ArrayBufferAllocator::Create();

	platform = node::MultiIsolatePlatform::Create(4);

	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();

	cppgc::InitializeProcess(platform->GetPageAllocator());

	isolate = node::NewIsolate(allocator.get(), uv_default_loop(), platform.get());

	{
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);
		v8::Local<v8::Context> context = node::NewContext(isolate);
		v8::Context::Scope context_scope(context);

		isolate_data = node::CreateIsolateData(isolate, uv_default_loop(), platform.get(), allocator.get());

		env = node::CreateEnvironment(isolate_data, context, args, exec_args);

		node::AddLinkedBinding(env, "gode", InitGodeAddon_C, NODE_API_DEFAULT_MODULE_API_VERSION);

		node::LoadEnvironment(env, "process._linkedBinding('gode');");

		node_context.Reset(isolate, context);
	}

	node_initialized = true;
}

void NodeRuntime::run_script(const std::string &code) {
	if (!node_initialized) {
		init_once();
	}

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);

	v8::Local<v8::Context> context = node_context.Get(isolate);
	v8::Context::Scope context_scope(context);

	v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, code.c_str(), v8::NewStringType::kNormal, static_cast<int>(code.size())).ToLocalChecked();
	v8::Local<v8::String> name = v8::String::NewFromUtf8(isolate, "<godot>", v8::NewStringType::kNormal).ToLocalChecked();

	v8::ScriptOrigin origin(name);
	v8::Local<v8::Script> script;

	if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
		return;
	}

	v8::MaybeLocal<v8::Value> result = script->Run(context);
	if (result.IsEmpty()) {
		return;
	}

	// Spin the event loop to ensure async operations complete if needed
	// Note: In a real game engine integration, you'd likely want to integrate
	// the event loop with the game loop rather than blocking here.
	// node::SpinEventLoop(env).ToChecked();
}

Napi::Value NodeRuntime::compile_script(const std::string &code, const std::string &filename) {
	if (!node_initialized) {
		init_once();
	}

	v8::Isolate::Scope isolate_scope(isolate);
	v8::EscapableHandleScope handle_scope(isolate);

	v8::Local<v8::Context> context = node_context.Get(isolate);
	v8::Context::Scope context_scope(context);

	// 1. Prepare the source code and origin
	v8::Local<v8::String> source_text = v8::String::NewFromUtf8(isolate, code.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
	v8::Local<v8::String> file_name = v8::String::NewFromUtf8(isolate, filename.c_str(), v8::NewStringType::kNormal).ToLocalChecked();

	v8::ScriptOrigin origin(file_name);
	v8::ScriptCompiler::Source source(source_text, origin);

	// 2. Define the argument names for the wrapper function
	//    (function (exports, require, module, __filename, __dirname) { ... })
	const char *arg_names[] = { "exports", "require", "module", "__filename", "__dirname" };
	int arg_count = 5;

	v8::Local<v8::String> params[5];
	for (int i = 0; i < arg_count; i++) {
		params[i] = v8::String::NewFromUtf8(isolate, arg_names[i], v8::NewStringType::kInternalized).ToLocalChecked();
	}

	// 3. Compile the function
	v8::MaybeLocal<v8::Function> maybe_fun = v8::ScriptCompiler::CompileFunction(
			context,
			&source,
			arg_count,
			params);

	v8::Local<v8::Function> compiled_fn;
	if (!maybe_fun.ToLocal(&compiled_fn)) {
		// Compilation failed (syntax error, etc.)
		return Napi::Value();
	}

	// 4. Create the 'module' and 'exports' objects
	v8::Local<v8::Object> module_obj = v8::Object::New(isolate);
	v8::Local<v8::Object> exports_obj = v8::Object::New(isolate);

	// module.exports = exports
	v8::Local<v8::String> exports_prop_name = v8::String::NewFromUtf8Literal(isolate, "exports");
	module_obj->Set(context, exports_prop_name, exports_obj).Check();

	// 5. Prepare the arguments values to pass to the function
	//    Order matches arg_names: [exports, require, module, __filename, __dirname]
	v8::Local<v8::Value> require_fn = v8::Undefined(isolate); // You can implement a custom require function here if needed
	v8::Local<v8::String> dirname = v8::String::NewFromUtf8Literal(isolate, "."); // Placeholder dirname

	v8::Local<v8::Value> args[] = {
		exports_obj, // exports
		require_fn, // require
		module_obj, // module
		file_name, // __filename
		dirname // __dirname
	};

	// 6. Run the compiled function
	v8::MaybeLocal<v8::Value> result = compiled_fn->Call(context, context->Global(), arg_count, args);

	if (result.IsEmpty()) {
		// Runtime error
		return Napi::Value();
	}

	// 7. Retrieve 'module.exports'
	//    The script might have done `module.exports = ...` replacing our original object
	v8::Local<v8::Value> final_exports;
	if (module_obj->Get(context, exports_prop_name).ToLocal(&final_exports)) {
		return Napi::Value(JsEnvManager::get_env(), reinterpret_cast<napi_value>(*handle_scope.Escape(final_exports)));
	}

	return Napi::Value();
}

Napi::Function NodeRuntime::get_default_class(Napi::Value module_exports) {
	if (module_exports.IsEmpty() || !module_exports.IsObject()) {
		return Napi::Function();
	}

	Napi::Object exports_obj = module_exports.As<Napi::Object>();
	if (exports_obj.Has("default")) {
		Napi::Value default_export = exports_obj.Get("default");
		if (default_export.IsFunction()) {
			return default_export.As<Napi::Function>();
		}
	}

	return Napi::Function();
}

void NodeRuntime::shutdown() {
	if (!node_initialized) {
		return;
	}

	if (env) {
		// Stop the environment first (this stops worker threads)
		// We need to be in the isolate scope to perform cleanup
		v8::Isolate::Scope isolate_scope(isolate);
		v8::HandleScope handle_scope(isolate);

		node::Stop(env);
		node::FreeEnvironment(env);
		env = nullptr;
	}

	node_context.Reset(); // Dispose context AFTER freeing environment

	if (isolate_data) {
		node::FreeIsolateData(isolate_data);
		isolate_data = nullptr;
	}

	if (isolate) {
		if (platform) {
			platform->UnregisterIsolate(isolate);
		}
		isolate->Dispose();
		isolate = nullptr;
	}

	v8::V8::Dispose();
	v8::V8::DisposePlatform();

	cppgc::ShutdownProcess();

	platform.reset();
	allocator.reset();

	node_initialized = false;
}

} // namespace gode
