# Gode

在 Godot 4（GDExtension）里嵌入 Node.js 运行时，并提供：
- JavaScript 作为 ScriptLanguage（.js 脚本资源）
- `require('godot')` 原生绑定（N-API）
- `res://` 路径的 `require()`/模块解析与 `fs` 访问适配
- 自动生成的 Godot API JS 绑定（Node/Resource/UtilityFunctions/Builtin types 等）
- **操作符重载支持**：内置类型（如 Vector2）支持算术和比较操作（如 `v1.add(v2)`, `v1.equal(v2)`）
- **信号绑定**：支持信号连接与发射（如 `node.connect("ready", cb)`, `node.signal_name`）
- **属性访问器**：支持直接读写对象属性（如 `node.name = "new_name"`）
- **toSignal 方法**：将信号转换为 Promise，支持 async/await

本仓库包含多个子模块：`godot-cpp`、`node-addon-api`、`node`（Node.js 源码）、`tree-sitter`、`tree-sitter-javascript`。

## 快速开始（跑 example）

1. 初始化子模块：
```bash
git submodule update --init --recursive
```

2. 构建扩展（见下方“构建”）。

3. 打开示例工程并运行：
- 用 Godot 打开 `example/`
- Project Settings → Plugins 启用 `gode`
- 直接运行工程（F5），主场景是 [node_2d.tscn](example/scene/node_2d.tscn)，其脚本是 [MyNode.js](example/script/MyNode.js)

## 版本要求

- Godot：示例扩展配置要求 `compatibility_minimum = "4.6.1"`（见 [.gdextension](example/addons/gode/bin/.gdextension)）
- CMake：≥ 3.12
- Python：用于代码生成（见 [code_generator/README.md](code_generator/README.md)）
- Windows：当前 CMakeLists 针对 Windows/libnode 的路径做了特殊处理（见 [CMakeLists.txt](CMakeLists.txt)）

## 构建（Windows / Visual Studio）

### 1) 准备 libnode（node/out/**）

本项目默认从 `node/` 子模块链接 `libnode`（DLL import lib + DLL）。

你需要先把 Node.js 编译出以下产物（默认路径）：
- `node/out/Debug/libnode.lib` 与 `node/out/Debug/libnode.dll`
- `node/out/Release/libnode.lib` 与 `node/out/Release/libnode.dll`

项目会在构建后自动把 `libnode.dll` 拷贝到 `example/addons/gode/bin/${CONFIG}/`（见 [CMakeLists.txt](CMakeLists.txt)）。

### 2) 生成 VS 工程并编译

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

构建完成后会自动把扩展 DLL 拷贝到：
- `example/addons/gode/bin/Debug/libgode.dll` 或 `bin/Release/libgode.dll`（见 [CMakeLists.txt](CMakeLists.txt)）

## 运行 example（Godot 编辑器）

### 1) 确认 .gdextension 指向正确 DLL

示例工程内的扩展描述文件是 [.gdextension](example/addons/gode/bin/.gdextension)，其中配置了 Debug/Release 的 DLL 路径：
- `res://addons/gode/bin/Debug/libgode.dll`
- `res://addons/gode/bin/Release/libgode.dll`

只要你按上面的 CMake 构建，POST_BUILD 会自动把产物拷贝到这个目录，一般不需要手动改。

### 2) 安装 JS 依赖（可选）

示例脚本 [MyNode.js](example/script/MyNode.js) 使用了 `is-odd`：
- 如果 `example/node_modules/` 已存在（仓库可能已带），可跳过
- 如果没有，请在 `example/` 下执行：

```bash
npm install
```

### 3) 启用插件与运行

1. 用 Godot 打开 `example/` 工程（[project.godot](example/project.godot)）。
2. Project Settings → Plugins 启用 `gode`（[plugin.cfg](example/addons/gode/plugin.cfg)）。
3. 插件会添加一个 Autoload 单例 `EventLoop`（见 [gode.gd](example/addons/gode/gode.gd) 与 [event_loop.gd](example/addons/gode/script/event_loop.gd)）。
4. 运行工程（F5）。主场景为 [node_2d.tscn](example/scene/node_2d.tscn)，挂载脚本为 `res://script/MyNode.js`。

## 功能示例

### 操作符重载

内置类型（如 `Vector2`, `Color`）支持类似 C++ 的操作符：

```javascript
let v1 = new Vector2(1, 2);
let v2 = new Vector2(3, 4);

// 算术运算
let v3 = v1.add(v2);      // (4, 6)
let v4 = v1.subtract(v2); // (-2, -2)
let v5 = v1.multiply(2);  // (2, 4)
let v6 = v1.multiply(v2); // (3, 8)

// 比较运算
if (v1.not_equal(v2)) {
    console.log("Vectors are different");
}
```

### 信号与属性

```javascript
// 属性访问器
let node = new Node();
node.name = "MyNode"; // 自动调用 set_name
console.log(node.name); // 自动调用 get_name

// 信号连接
node.connect("renamed", () => {
    console.log("Node renamed!");
});

// 或者使用信号对象属性
node.renamed.connect(() => {
    console.log("Node renamed via signal object!");
});

node.name = "NewName"; // 触发信号
```

### toSignal 方法

Godot 对象现在支持 `toSignal()` 方法，可以将信号转换为 Promise，方便使用 async/await：

```javascript
// 等待信号触发
async function waitForReady(node) {
    await node.toSignal("ready");
    console.log("Node is ready!");
}

// 等待带参数的信号
async function waitForBodyEntered(area) {
    const body = await area.toSignal("body_entered");
    console.log("Body entered:", body);
}
```

## JavaScript / res:// 模块加载机制

项目会对 Node 的模块系统做一层注入与兼容，使以下行为成立：
- `require('godot')` 返回原生绑定对象（来自 `process._linkedBinding('godot')`）
- `fs.readFileSync('res://...')` / `fs.existsSync('res://...')` / `fs.statSync('res://...')` 走 Godot 侧文件接口
- `require('./x')`、`require('pkg')` 在 `res://` 场景下具备类似 Node 的解析规则（扩展名、package.json main、index.js、node_modules 向上查找）

对应实现集中在 [node_runtime.cpp](src/utils/node_runtime.cpp)：
- 注入 `godot` 模块到 `Module._cache`
- 重写 `fs.readFileSync/existsSync/statSync`
- 重写 `path.join/resolve/dirname` 以兼容 `res://`
- 重写 `Module._nodeModulePaths/_findPath/_resolveFilename`
- 提供 `globalThis.__gode_compile(code, filename)` 进行按文件名上下文编译

## 绑定生成（code_generator）

此仓库包含一个 Python + Jinja2 的绑定生成器，用于生成：
- `include/generated/**`
- `src/generated/**`

使用方法见 [code_generator/README.md](code_generator/README.md)。

常用命令：
```bash
pip install -r code_generator/requirements.txt
python code_generator/generator.py
```

## 对象生命周期与 GC 约定（JS ↔ Godot）（简要）

### 1) Godot 对象销毁后，JS 访问安全

生成的 `*Binding` 现在会保存 `ObjectID`，每次调用前通过 `ObjectDB::get_instance(id)` 校验对象是否仍存在；对象已销毁时抛出 JS 异常，避免悬空指针崩溃。

### 2) JS GC 时，Node 的“是否回收实例”策略

当 JS 侧对象被 GC 触发 `~*Binding()`：
- 若是 `Node`（或其子类）且 **不在 SceneTree**（`!node->is_inside_tree()`），则调用 `queue_free()` 释放 Godot 侧实例
- 若已在 SceneTree，则仅回收 JS 包装对象，Godot 侧由场景树继续管理

### 3) ScriptInstance 释放 JS 强引用

当一个挂载了 JS Script 的 Godot 对象被 `queue_free()`：
- Godot 会销毁其 ScriptInstance
- `JavascriptInstance` 析构中会 `Reset()` 掉 `Napi::ObjectReference`，解除 C++→JS 的强引用，随后 JS 对象可被 GC

相关实现见：
- [javascript_instance.cpp](src/support/javascript/javascript_instance.cpp)

## 贡献指南

欢迎提交 Pull Request。请注意以下事项：

- **不要使用 AI 工具（如 Claude、Copilot）直接代劳提交代码。**
  AI 代劳提交的 commit 通常会在提交信息中附带 `Co-Authored-By: Claude` 等字样，导致 AI 工具作为共同作者出现在 git 历史中。如果你使用 AI 辅助编写代码，这完全没问题——但最终提交时，请确保 commit 仅由你本人署名，不含任何 AI 共同作者标记。

## 目录结构（简要）

- `src/` / `include/`：扩展主体
- `src/generated/` / `include/generated/`：自动生成的绑定代码
- `code_generator/`：Python 代码生成器
- `example/`：Godot 示例工程（包含插件与 .gdextension 配置）
- `node/`：Node.js 源码子模块（用于构建 libnode）
- `node-addon-api/`：N-API C++ 封装头文件子模块
- `tree-sitter*`：用于 JS 相关解析/语法支持的子模块
