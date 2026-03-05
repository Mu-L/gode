const godot = require('godot');
const isOdd = require('is-odd');
const BaseClass = require('./BaseClass.js');

class MyNode extends BaseClass {
	_ready() {
		GD.print("ready");
		console.log("base_value: ", this.base_value);
		GD.print('Is 1 odd?', isOdd(1));
		GD.print('Is 2 odd?', isOdd(2));
	}
}

module.exports = { default: MyNode };
