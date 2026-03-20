# Gode

Embed Node.js runtime in Godot 4 (GDExtension) with:
- JavaScript as ScriptLanguage (.js script resources)
- `require('godot')` native binding (N-API)
- `require()` module resolution and `fs` access for `res://` paths
- Auto-generated Godot API JS bindings (Node/Resource/UtilityFunctions/Builtin types, etc.)
- **Operator overloading**: Built-in types (e.g., Vector2) support arithmetic and comparison operations (e.g., `v1.add(v2)`, `v1.equal(v2)`)
- **Signal binding**: Support for signal connection and emission (e.g., `node.connect("ready", cb)`, `node.signal_name`)
- **Property accessors**: Direct read/write access to object properties (e.g., `node.name = "new_name"`)
- **toSignal method**: Convert signals to Promises for async/await support

This repository contains multiple submodules: `godot-cpp`, `node-addon-api`, `node` (Node.js source), `tree-sitter`, `tree-sitter-javascript`.

## Quick Start (Running the Example)

1. Initialize submodules:
```bash
git submodule update --init --recursive
```

2. Build the extension (see "Build" section below).

3. Open and run the example project:
- Open `example/` in Godot
- Enable the `gode` plugin in Project Settings → Plugins
- Run the project (F5). The main scene is [node_2d.tscn](example/scene/node_2d.tscn) with script [MyNode.js](example/script/MyNode.js)

## Requirements

- Godot: Example extension requires `compatibility_minimum = "4.6.1"` (see [.gdextension](example/addons/gode/bin/.gdextension))
- CMake: ≥ 3.12
- Python: For code generation (see [code_generator/README.md](code_generator/README.md))
- Windows: Current CMakeLists has special handling for Windows/libnode paths (see [CMakeLists.txt](CMakeLists.txt))

## Build (Windows / Visual Studio)

### 1) Prepare libnode (node/out/**)

This project links against `libnode` from the `node/` submodule by default.

You need to build Node.js first to produce (default paths):
- `node/out/Debug/libnode.lib` and `node/out/Debug/libnode.dll`
- `node/out/Release/libnode.lib` and `node/out/Release/libnode.dll`

The project will automatically copy `libnode.dll` to `example/addons/gode/bin/${CONFIG}/` after build (see [CMakeLists.txt](CMakeLists.txt)).

### 2) Generate VS project and compile

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

After building, the extension DLL will be automatically copied to:
- `example/addons/gode/bin/Debug/libgode.dll` or `bin/Release/libgode.dll` (see [CMakeLists.txt](CMakeLists.txt))

## Running the Example (Godot Editor)

### 1) Verify .gdextension points to correct DLL

The extension descriptor file in the example project is [.gdextension](example/addons/gode/bin/.gdextension), which configures Debug/Release DLL paths:
- `res://addons/gode/bin/Debug/libgode.dll`
- `res://addons/gode/bin/Release/libgode.dll`

If you follow the CMake build steps above, POST_BUILD will automatically copy the artifacts to this directory, so manual changes are usually not needed.

### 2) Install JS dependencies (optional)

The example script [MyNode.js](example/script/MyNode.js) uses `is-odd`:
- If `example/node_modules/` already exists (repository may include it), you can skip this
- If not, run in `example/`:

```bash
npm install
```

### 3) Enable plugin and run

1. Open the `example/` project in Godot ([project.godot](example/project.godot)).
2. Enable the `gode` plugin in Project Settings → Plugins ([plugin.cfg](example/addons/gode/plugin.cfg)).
3. The plugin adds an Autoload singleton `EventLoop` (see [gode.gd](example/addons/gode/gode.gd) and [event_loop.gd](example/addons/gode/script/event_loop.gd)).
4. Run the project (F5). The main scene is [node_2d.tscn](example/scene/node_2d.tscn) with script `res://script/MyNode.js`.

## Feature Examples

### Operator Overloading

Built-in types (e.g., `Vector2`, `Color`) support C++-like operators:

```javascript
let v1 = new Vector2(1, 2);
let v2 = new Vector2(3, 4);

// Arithmetic operations
let v3 = v1.add(v2);      // (4, 6)
let v4 = v1.subtract(v2); // (-2, -2)
let v5 = v1.multiply(2);  // (2, 4)
let v6 = v1.multiply(v2); // (3, 8)

// Comparison operations
if (v1.not_equal(v2)) {
    console.log("Vectors are different");
}
```

### Signals and Properties

```javascript
// Property accessors
let node = new Node();
node.name = "MyNode"; // Automatically calls set_name
console.log(node.name); // Automatically calls get_name

// Signal connection
node.connect("renamed", () => {
    console.log("Node renamed!");
});

// Or use signal object property
node.renamed.connect(() => {
    console.log("Node renamed via signal object!");
});

node.name = "NewName"; // Triggers signal
```

### toSignal Method

Godot objects now support the `toSignal()` method, which converts signals to Promises for convenient async/await usage:

```javascript
// Wait for signal to trigger
async function waitForReady(node) {
    await node.toSignal("ready");
    console.log("Node is ready!");
}

// Wait for signal with parameters
async function waitForBodyEntered(area) {
    const body = await area.toSignal("body_entered");
    console.log("Body entered:", body);
}
```

## JavaScript / res:// Module Loading

The project injects and adapts Node's module system to enable:
- `require('godot')` returns native binding object (from `process._linkedBinding('godot')`)
- `fs.readFileSync('res://...')` / `fs.existsSync('res://...')` / `fs.statSync('res://...')` use Godot's file interface
- `require('./x')`, `require('pkg')` in `res://` context follow Node-like resolution rules (extensions, package.json main, index.js, node_modules lookup)

Implementation is concentrated in [node_runtime.cpp](src/utils/node_runtime.cpp):
- Inject `godot` module into `Module._cache`
- Override `fs.readFileSync/existsSync/statSync`
- Override `path.join/resolve/dirname` for `res://` compatibility
- Override `Module._nodeModulePaths/_findPath/_resolveFilename`
- Provide `globalThis.__gode_compile(code, filename)` for context-aware compilation

## Binding Generation (code_generator)

This repository includes a Python + Jinja2 binding generator that produces:
- `include/generated/**`
- `src/generated/**`

See [code_generator/README.md](code_generator/README.md) for usage.

Common commands:
```bash
pip install -r code_generator/requirements.txt
python code_generator/generator.py
```

## Object Lifecycle & GC Conventions (JS ↔ Godot) (Brief)

### 1) Safe JS access after Godot object destruction

Generated `*Binding` classes now store `ObjectID` and validate object existence via `ObjectDB::get_instance(id)` before each call; throws JS exception if object is destroyed, preventing dangling pointer crashes.

### 2) JS GC and Node instance reclamation strategy

When JS-side object is GC'd and triggers `~*Binding()`:
- If it's a `Node` (or subclass) and **not in SceneTree** (`!node->is_inside_tree()`), calls `queue_free()` to release Godot-side instance
- If already in SceneTree, only reclaims JS wrapper; Godot-side continues to be managed by scene tree

### 3) ScriptInstance releases JS strong reference

When a Godot object with JS Script is `queue_free()`'d:
- Godot destroys its ScriptInstance
- `JavascriptInstance` destructor `Reset()`s the `Napi::ObjectReference`, breaking C++→JS strong reference, allowing JS object to be GC'd

Related implementation: [javascript_instance.cpp](src/support/javascript/javascript_instance.cpp)

## Contributing

Pull requests are welcome. Please keep the following in mind:

- **Do not use AI assistants (e.g. Claude, Copilot) to commit code on your behalf.**
  AI-generated commits often include `Co-Authored-By: Claude` or similar lines in the commit message, which adds AI tools as co-authors in the git history. If you used an AI to help write code, that is fine — just make sure the final commit is authored by you alone, without any AI co-author trailer.



- `src/` / `include/`: Extension core
- `src/generated/` / `include/generated/`: Auto-generated binding code
- `code_generator/`: Python code generator
- `example/`: Godot example project (includes plugin and .gdextension config)
- `node/`: Node.js source submodule (for building libnode)
- `node-addon-api/`: N-API C++ wrapper headers submodule
- `tree-sitter*`: Submodules for JS parsing/syntax support
