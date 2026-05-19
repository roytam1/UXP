// |reftest| skip-if(!String.prototype.isWellFormed||!String.prototype.toWellFormed)

assertEq("".isWellFormed(), true);
assertEq("abc".isWellFormed(), true);
assertEq("\uD83D\uDE00".isWellFormed(), true);
assertEq("\uD800".isWellFormed(), false);
assertEq("\uDC00".isWellFormed(), false);
assertEq("\uD800a".isWellFormed(), false);
assertEq("a\uDC00".isWellFormed(), false);
assertEq("\uD800\uD800\uDC00".isWellFormed(), false);

assertEq("abc".toWellFormed(), "abc");
assertEq("\uD83D\uDE00".toWellFormed(), "\uD83D\uDE00");
assertEq("\uD800".toWellFormed(), "\uFFFD");
assertEq("\uDC00".toWellFormed(), "\uFFFD");
assertEq("\uD800a\uDC00".toWellFormed(), "\uFFFDa\uFFFD");
assertEq("\uD800\uD800\uDC00".toWellFormed(), "\uFFFD\uD800\uDC00");

assertEq(String.prototype.isWellFormed.call(123), true);
assertEq(String.prototype.toWellFormed.call({ toString() { return "\uD800x"; } }), "\uFFFDx");

assertThrowsInstanceOf(() => String.prototype.isWellFormed.call(null), TypeError);
assertThrowsInstanceOf(() => String.prototype.toWellFormed.call(undefined), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
