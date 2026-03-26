import { Control } from "godot";

export default class Test extends Control {
	static exports: ExportMap = {
		_hello: {
			type: "string",
		},
	};

	_hello: string = "hello";
 
	_ready(): void {
		GD.print(this._hello);
	} 
}
