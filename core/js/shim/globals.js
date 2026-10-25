// Obsidian 给内置类型加的扩展方法，原版插件代码直接在用（与 Obsidian 行为一致）
const def = (proto, name, fn) => { if (!proto[name]) Object.defineProperty(proto, name, { value: fn, configurable: true, writable: true }); };
def(String.prototype, "contains", function (s) { return this.includes(s); });
def(Array.prototype, "contains", function (s) { return this.includes(s); });
def(Array.prototype, "first", function () { return this[0]; });
def(Array.prototype, "last", function () { return this[this.length - 1]; });
def(Array.prototype, "remove", function (x) { for (let i = this.length - 1; i >= 0; i--) if (this[i] === x) this.splice(i, 1); });
if (typeof globalThis.window === "undefined") globalThis.window = globalThis;
