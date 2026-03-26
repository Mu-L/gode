#include "support/typescript/typescript.h"
#include "support/typescript/typescript_language.h"
#include "utils/node_runtime.h"
#include "utils/value_convert.h"

#include <tree_sitter/api.h>
#include <v8-isolate.h>
#include <v8-locker.h>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;
using namespace gode;

extern "C" const TSLanguage *tree_sitter_typescript();

static String get_js_path(const String &ts_path) {
	String rel = ts_path;
	if (rel.begins_with("res://")) {
		rel = rel.substr(6);
	}
	return "res://dist/" + rel.get_basename() + ".js";
}

bool Typescript::compile() const {
	if (!is_dirty) {
		return true;
	}

	String path = get_path();
	if (path.is_empty()) {
		return false;
	}

	String js_path = get_js_path(path);
	String js_code;
	if (FileAccess::file_exists(js_path)) {
		js_code = FileAccess::get_file_as_string(js_path);
	} else {
		return false;
	}

	v8::Locker locker(NodeRuntime::isolate);
	v8::Isolate::Scope isolate_scope(NodeRuntime::isolate);
	v8::HandleScope handle_scope(NodeRuntime::isolate);

	Napi::Value exports = NodeRuntime::compile_script(js_code.utf8().get_data(), js_path.utf8().get_data());
	Napi::Function cls = NodeRuntime::get_default_class(exports);

	if (cls.IsEmpty() || cls.IsUndefined() || cls.IsNull()) {
		default_class.Reset();
		return false;
	}

	default_class = Napi::Persistent(cls);

	// tree-sitter 解析 .ts 源码元数据
	TSParser *parser = ts_parser_new();
	ts_parser_set_language(parser, tree_sitter_typescript());

	std::string source = source_code.utf8().get_data();
	TSTree *tree = ts_parser_parse_string(parser, NULL, source.c_str(), source.length());
	TSNode root_node = ts_tree_root_node(tree);

	class_name = StringName();
	base_class_name = StringName();
	methods.clear();
	signals.clear();
	properties.clear();
	property_defaults.clear();
	constants.clear();
	member_lines.clear();
	is_tool_script = false;

	// Parse static exports from AST
	auto parse_exports = [&](TSNode body_node) {
		uint32_t mc = ts_node_child_count(body_node);
		for (uint32_t j = 0; j < mc; j++) {
			TSNode member = ts_node_child(body_node, j);
			if (strcmp(ts_node_type(member), "public_field_definition") != 0) continue;

			bool is_static = false;
			TSNode name_node = { 0 };
			TSNode value_node = { 0 };

			uint32_t fc = ts_node_child_count(member);
			for (uint32_t k = 0; k < fc; k++) {
				TSNode child = ts_node_child(member, k);
				const char *ct = ts_node_type(child);
				if (strcmp(ct, "static") == 0) is_static = true;
				else if (strcmp(ct, "property_identifier") == 0) name_node = child;
				else if (strcmp(ct, "object") == 0) value_node = child;
			}

			if (!is_static || ts_node_is_null(name_node) || ts_node_is_null(value_node)) continue;

			uint32_t ns = ts_node_start_byte(name_node);
			uint32_t ne = ts_node_end_byte(name_node);
			std::string field_name = source.substr(ns, ne - ns);
			if (field_name != "exports") continue;

			uint32_t oc = ts_node_child_count(value_node);
			for (uint32_t m = 0; m < oc; m++) {
				TSNode pair = ts_node_child(value_node, m);
				if (strcmp(ts_node_type(pair), "pair") != 0) continue;

				TSNode key_node = ts_node_child_by_field_name(pair, "key", 3);
				TSNode val_node = ts_node_child_by_field_name(pair, "value", 5);
				if (ts_node_is_null(key_node) || ts_node_is_null(val_node) || strcmp(ts_node_type(val_node), "object") != 0) continue;

				uint32_t ks = ts_node_start_byte(key_node);
				uint32_t ke = ts_node_end_byte(key_node);
				std::string prop_name = source.substr(ks, ke - ks);

				PropertyInfo pi;
				pi.name = StringName(prop_name.c_str());
				pi.usage = PROPERTY_USAGE_DEFAULT;
				pi.hint = PROPERTY_HINT_NONE;
				pi.type = Variant::NIL;

				uint32_t pc = ts_node_child_count(val_node);
				for (uint32_t n = 0; n < pc; n++) {
					TSNode prop_pair = ts_node_child(val_node, n);
					if (strcmp(ts_node_type(prop_pair), "pair") != 0) continue;

					TSNode pk = ts_node_child_by_field_name(prop_pair, "key", 3);
					TSNode pv = ts_node_child_by_field_name(prop_pair, "value", 5);
					if (ts_node_is_null(pk) || ts_node_is_null(pv)) continue;

					uint32_t pks = ts_node_start_byte(pk);
					uint32_t pke = ts_node_end_byte(pk);
					std::string field_key = source.substr(pks, pke - pks);

					if (field_key == "type" && strcmp(ts_node_type(pv), "string") == 0) {
						uint32_t pvs = ts_node_start_byte(pv) + 1;
						uint32_t pve = ts_node_end_byte(pv) - 1;
						std::string type_str = source.substr(pvs, pve - pvs);
						if (type_str == "bool") pi.type = Variant::BOOL;
						else if (type_str == "int") pi.type = Variant::INT;
						else if (type_str == "float" || type_str == "number") pi.type = Variant::FLOAT;
						else if (type_str == "String" || type_str == "string") pi.type = Variant::STRING;
						else if (type_str == "Vector2") pi.type = Variant::VECTOR2;
						else if (type_str == "Vector2i") pi.type = Variant::VECTOR2I;
						else if (type_str == "Vector3") pi.type = Variant::VECTOR3;
						else if (type_str == "Vector3i") pi.type = Variant::VECTOR3I;
						else if (type_str == "Vector4") pi.type = Variant::VECTOR4;
						else if (type_str == "Vector4i") pi.type = Variant::VECTOR4I;
						else if (type_str == "Color") pi.type = Variant::COLOR;
						else if (type_str == "NodePath") pi.type = Variant::NODE_PATH;
						else if (type_str == "Object") pi.type = Variant::OBJECT;
					} else if (field_key == "hint" && strcmp(ts_node_type(pv), "number") == 0) {
						uint32_t pvs = ts_node_start_byte(pv);
						uint32_t pve = ts_node_end_byte(pv);
						pi.hint = (PropertyHint)std::stoi(source.substr(pvs, pve - pvs));
					} else if (field_key == "hint_string" && strcmp(ts_node_type(pv), "string") == 0) {
						uint32_t pvs = ts_node_start_byte(pv) + 1;
						uint32_t pve = ts_node_end_byte(pv) - 1;
						pi.hint_string = String(source.substr(pvs, pve - pvs).c_str());
					}
				}

				properties[pi.name] = pi;
			}
		}
	};

	// Read static tool — equivalent to @tool in GDScript
	// TS usage: static tool = true
	if (cls.Has("tool") && cls.Get("tool").IsBoolean()) {
		is_tool_script = cls.Get("tool").As<Napi::Boolean>().Value();
	}

	// 解析 AST：export_statement > export default class / class_declaration
	uint32_t child_count = ts_node_child_count(root_node);
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_child(root_node, i);
		const char *node_type = ts_node_type(child);

		TSNode class_node = { 0 };
		if (strcmp(node_type, "export_statement") == 0) {
			uint32_t ec = ts_node_child_count(child);
			for (uint32_t j = 0; j < ec; j++) {
				TSNode en = ts_node_child(child, j);
				if (strcmp(ts_node_type(en), "class_declaration") == 0) {
					class_node = en;
					break;
				}
			}
		} else if (strcmp(node_type, "class_declaration") == 0) {
			class_node = child;
		}

		if (ts_node_is_null(class_node)) continue;

		// class name
		TSNode name_node = ts_node_child_by_field_name(class_node, "name", strlen("name"));
		if (!ts_node_is_null(name_node)) {
			uint32_t start = ts_node_start_byte(name_node);
			uint32_t end = ts_node_end_byte(name_node);
			class_name = StringName(source.substr(start, end - start).c_str());
		}

		// base class (extends clause)
		TSNode body_node = ts_node_child_by_field_name(class_node, "body", strlen("body"));
		uint32_t cc = ts_node_child_count(class_node);
		for (uint32_t j = 0; j < cc; j++) {
			TSNode cn = ts_node_child(class_node, j);
			if (strcmp(ts_node_type(cn), "class_heritage") == 0) {
				uint32_t hc = ts_node_child_count(cn);
				for (uint32_t k = 0; k < hc; k++) {
					TSNode hn = ts_node_child(cn, k);
					if (strcmp(ts_node_type(hn), "identifier") == 0) {
						uint32_t s = ts_node_start_byte(hn);
						uint32_t e = ts_node_end_byte(hn);
						base_class_name = StringName(source.substr(s, e - s).c_str());
						break;
					}
				}
			}
		}

		// methods and exports from class body
		if (!ts_node_is_null(body_node)) {
			parse_exports(body_node);

			uint32_t mc = ts_node_child_count(body_node);
			for (uint32_t j = 0; j < mc; j++) {
				TSNode member = ts_node_child(body_node, j);
				const char *member_type = ts_node_type(member);

				if (strcmp(member_type, "public_field_definition") == 0) {
					TSNode field_name_node = ts_node_child_by_field_name(member, "name", 4);
					TSNode field_value_node = ts_node_child_by_field_name(member, "value", 5);

					if (!ts_node_is_null(field_name_node) && !ts_node_is_null(field_value_node)) {
						uint32_t fns = ts_node_start_byte(field_name_node);
						uint32_t fne = ts_node_end_byte(field_name_node);
						StringName field_name(source.substr(fns, fne - fns).c_str());

						if (properties.has(field_name)) {
							const char *vt = ts_node_type(field_value_node);
							uint32_t vs = ts_node_start_byte(field_value_node);
							uint32_t ve = ts_node_end_byte(field_value_node);

							if (strcmp(vt, "string") == 0) {
								property_defaults[field_name] = String(source.substr(vs + 1, ve - vs - 2).c_str());
							} else if (strcmp(vt, "number") == 0) {
								std::string num_str = source.substr(vs, ve - vs);
								if (properties[field_name].type == Variant::INT) property_defaults[field_name] = std::stoi(num_str);
								else property_defaults[field_name] = std::stod(num_str);
							} else if (strcmp(vt, "true") == 0) {
								property_defaults[field_name] = true;
							} else if (strcmp(vt, "false") == 0) {
								property_defaults[field_name] = false;
							}
						}
					}
					continue;
				}

				if (strcmp(member_type, "method_definition") != 0) continue;

				TSNode mn = ts_node_child_by_field_name(member, "name", strlen("name"));
				if (ts_node_is_null(mn)) continue;
				uint32_t s = ts_node_start_byte(mn);
				uint32_t e = ts_node_end_byte(mn);
				StringName method_name(source.substr(s, e - s).c_str());

				bool is_static = false;
				uint32_t mcc = ts_node_child_count(member);
				for (uint32_t k = 0; k < mcc; k++) {
					if (strcmp(ts_node_type(ts_node_child(member, k)), "static") == 0) {
						is_static = true;
						break;
					}
				}

				MethodInfo mi;
				mi.name = method_name;
				if (is_static) mi.flags |= METHOD_FLAG_STATIC;
				methods[method_name] = mi;
				member_lines[method_name] = ts_node_start_point(member).row + 1;
			}
		}

		break;
	}

	ts_tree_delete(tree);
	ts_parser_delete(parser);

	is_valid = true;
	is_dirty = false;
	return true;
}

ScriptLanguage *Typescript::_get_language() const {
	return TypescriptLanguage::get_singleton();
}
