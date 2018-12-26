# UXP Coding Style Guide
While our source tree is currently in a number of different Coding Styles and the general rule applies to adhere to style of surrounding code when you are making changes, it is our goal to unify the style of our source tree and making it adhere to a single style.
This document describes the preferred style for new source code files and the preferred style to convert existing files to.

This style guide will not apply to 3rd party libraries that are imported verbatim to our code (e.g. NSS, NSPR, SQLite, various media libs, etc.).

Our own managed and maintained code should adhere to this guide where possible or feasible. It departs from the Mozilla Coding Style in some ways, but considering Mozilla has abandoned their code style in favor of Google code style, we are now defining our own preferred style.
## General formatting rules
The following formatting rules apply to all code:
- Always use spaces for indentation, never use tabs!
- Put a space between a keyword and parenthesis, e.g. `if (`.
- Put a space between variables and operators, e.g. `a == b`.
- Put a space after a comma in variable lists, e.g. `function (a, b, c)`.
- Indentation of scopes is 2 spaces.
- Indentation of long lines is variable-aligned or expression-aligned (see "long line wrapping")
- Conditional defines are always placed on column 1. This is also true for nested defines.
- Maximum line length is 120 characters. This departs from the often-used 80-character limit which is rooted in the archaic use of 80-column text terminals and should no longer apply in this day.
- Variables passed to functions are passed on one line unless the line would exceed the maximum length, in which case variables will be passed 1-per-line, expression-aligned (see "long line wrapping").
- Avoid trailing commas.
- Comment blocks are line-quoted if they appear within functions.
- Comment blocks are block-quoted if they appear outside of functions (e.g. as function definition headers).

## C and C++
Applies to `*.c`, `*.cc`, `*.cpp` and `*.h`. Potentially also `*.mm` for Mac code.
### Flow control
Flow control expressions should follow the following guidelines:
- Scopes have their opening braces on the expression line
- Scopes have their closing braces on a separate line, indent-aligned with the flow control expression
- Any alternative flow control paths are generally started with an expression on the closing brace line
- Case statements are indent-aligned with the case expression content directly following the case.
- Flow control default scopes are always placed at the bottom.
#### if..else
`if..else` statements example:
```C++
if (something == somethingelse) {
  true_case_code here;
} else {
  false_case_code here;
}
```
#### for
`for` loop example:
```C++
for (i = 1; i < max; i++) {
  loop_code here;
}
```
#### while
`while` loop example:
```C++
while (something == true) {
  loop_code here;
}
```
#### switch..case
`switch..case` flow control example:
```C++
switch (variable) {
  case value1: code_for_1;
               code_for_1;
               break;
  case value2: code_for_2;
               code_for_2;
               // fallthrough
  case value3: code_for_3;
               break;
  default: code_for_default;
           code_for_default;
}           
```
## JavaScript
Applies to `*.js` and `*.jsm`.
## XUL and other XML-derivatives 
Applies to `*.xul`, `*.html`, `*.xhtml`.
## IDL
Applies to `*.idl`, `*.xpidl` and `*.webidl`.

