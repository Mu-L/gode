import { Control } from 'godot';

interface Stats {
	health: number;
	speed: number;
}

interface MyiInterface {
	a: number;
	b: string;
	stats: Stats;
}

export default class Test extends Control {
	@Export
	v: string = "hello";

	@Export
	i: MyiInterface = {
		a: 1,
		b: "world",
		stats: { health: 100, speed: 5 }
	};

	private pass = 0;
	private fail = 0;

	private assert(name: string, actual: any, expected: any): void {
		if (actual === expected) {
			GD.print(`  [PASS] ${name}`);
			this.pass++;
		} else {
			GD.print(`  [FAIL] ${name}: expected ${expected}, got ${actual}`);
			this.fail++;
		}
	}

	_ready(): void {
		GD.print("=== Test: @Export 基础属性 ===");
		this.assert("v 初始值", this.v, "hello");

		GD.print("=== Test: @Export interface 展开 ===");
		this.assert("i::a 初始值", this.i.a, 1);
		this.assert("i::b 初始值", this.i.b, "world");

		GD.print("=== Test: @Export 嵌套 interface (subgroup) ===");
		this.assert("i::stats::health 初始值", this.i.stats.health, 100);
		this.assert("i::stats::speed 初始值", this.i.stats.speed, 5);

		GD.print("=== Test: 写入属性后读取 ===");
		this.v = "updated";
		this.assert("v 写入后", this.v, "updated");

		this.i = { ...this.i, a: 42 };
		this.assert("i::a 写入后", this.i.a, 42);

		this.i = { ...this.i, stats: { ...this.i.stats, health: 200 } };
		this.assert("i::stats::health 写入后", this.i.stats.health, 200);

		GD.print(`=== 结果: ${this.pass} passed, ${this.fail} failed ===`);
	}
}
