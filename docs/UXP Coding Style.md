# UXP Coding Style Guide
While our source tree is currently in a number of different Coding Styles and the general rule applies to adhere to style of surrounding code when you are making changes, it is our goal to unify the style of our source tree and making it adhere to a single style.
This document describes the preferred style for new source code files and the preferred style to convert existing files to.

This style guide will not apply to 3rd party libraries that are imported verbatim to our code (e.g. NSS, NSPR, SQLite, various media libs, etc.).

Our own managed and maintained code should adhere to this guide where possible or feasible. It departs from the Mozilla Coding Style in some ways, but considering Mozilla has abandoned their code style in favor of Google code style, we are now defining our own preferred style.

**Important**: if you touch a file to make it adhere to this code style, do not mix code changes with formatting changes. Always make formatting changes throughout a file a single, separate commit that does not touch code functionality!

## General formatting rules
The following formatting rules apply to all code:
- Always use spaces for indentation, never use tabs!
- Put a space between a keyword and parenthesis, e.g. `if (`.
- Put a space between variables and operators, e.g. `a == b`.
- Put a space after a comma or semicolon in variable lists, e.g. `function (a, b, c)` or `for (i = 1; i < 10; i++)`.
- Indentation of scopes is 2 spaces.
- Indentation of long lines is variable-aligned or expression-aligned (see "long line wrapping")
- Conditional defines are always placed on column 1. This is also true for nested defines.
- Maximum line length is 120 characters. This departs from the often-used 80-character limit which is rooted in the archaic use of 80-column text terminals and should no longer apply in this day. Github, our chief web VCS, has no issues dealing with 120 character lines either.
- Variables passed to functions are passed on one line unless the line would exceed the maximum length, in which case variables will be passed 1-per-line, expression-aligned (see "long line wrapping").
- Avoid trailing commas.
- Comment blocks are line-quoted if they appear within functions.
- Comment blocks are block-quoted if they appear outside of functions (e.g. as function definition headers).

## C and C++
Applies to `*.c`, `*.cc`, `*.cpp` and `*.h`. Potentially also `*.mm` for Mac code.
### General formatting guidelines
- Place function return types, including modifiers like `static` on the same line as the function signature.
  ```C++
  static bool somefunction(int var1, char* var2) {
    ...
  }
  ```
- When using templates, do not add spaces between the template name and its object.
  ```C++
  nsCOMPtr<nsISomeInterfaceType>
  ```
- Use braces, even for single-command lines. Avoid placing executed code on the expression line or using single-line indentation to indicate the scope. Always brace executed code.

  WRONG:
  ```C++
  if (a == b) do_something();
  if (a == b)
    do_something();
  if (a == b) { do_something(); }
  ```
  CORRECT:
  ```C++
  if (a == b) {
    do_something();
  }
  ```
- Pointer types: When declaring pointer types, place the `*` with the pointer type. When referencing pointer types, place `*` or `&` with the variable. Do not use `*` by itself with whitespace. Some existing modules still use the `int *myvar1;` style, these should be converted. Until conversion is done, follow existing, surrounding code style or convert it when you touch a file.

  WRONG:
  ```C++
  int * myvar;
  int *myvar1, *myvar2;
  int* myvar1, *myvar2;
  int* myvar1, myvar2; //myvar2 isn't a pointer!
  ```
  CORRECT:
  ```C++
  char* somePointer;
  int* myvar1;
  int* myvar2;
  function a(char* somePointer) [...]
  myString = *somePointer;
  myPointer = &myString;
  ```

### Flow control
Flow control expressions should follow the following guidelines:
- Scopes have their opening braces on the expression line
- Scopes have their closing braces on a separate line, indent-aligned with the flow control expression
- Any alternative flow control paths are generally started with an expression on the closing brace line
- Case statements are indented by 2 on a new line with the case expression on its own line.
- Flow control default scopes are always placed at the bottom.
#### if..else
`if..else` statements example:
```C++
if (something == somethingelse) {
  true_case_code here;
} else {
  false_case_code here;
}

if (case1) {
  case1_code here;
} else if (case2) {
  case2_code here;
} else {
  other_case_code here;
}

if (case1) {
  case1_code here;
} else {
  if (case2) {
    case2_code here;
  }
  case2_and_other_code here;
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
  case value1:
    // Comment describing 1
    code_for_1;
    code_for_1;
    break;
  case value2:
    code_for_2;
    code_for_2;
    // fallthrough
  case value3:
  case value4:
    code_for_3_and_4;
    break;
  default:
    code_for_default;
    code_for_default;
}
```
### Classes
Classes have some special formatting rules applied to them:
- Classes follow the following sequence: CTOR, DTOR, methods, variables
- Class CTOR variable initializers (in lieu of assignments in the CTOR body) are listed one-per-line with a leading comma (or colon for the first).
- `public`, `private` and `protected` keywords are not indented and are at the same indentation level as the class identifier statement.

Example class definition:
```C++
class FocusClassExample : public Runnable {
public:
  FocusClassExample(nsISupports* aTarget,
                    EventMessage aEventMessage,
                    nsPresContext* aContext,
                    nsPIDOMWindowOuter* aOriginalFocusedWindow,
                    nsIContent* aOriginalFocusedContent,
                    EventTarget* aRelatedTarget)
    : mTarget(aTarget)
    , mContext(aContext)
    , mEventMessage(aEventMessage)
    , mOriginalFocusedWindow(aOriginalFocusedWindow)
    , mOriginalFocusedContent(aOriginalFocusedContent)
    , mRelatedTarget(aRelatedTarget) {
    CTOR_initializer_code here
  }
  
  ~FocusClassExample {
    DTOR_code here
  }

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIContent> originalWindowFocus = mOriginalFocusedWindow ?        
                                               mOriginalFocusedWindow->GetFocusedNode() :
                                               nullptr;
    // Blink does not check that focus is the same after blur, but WebKit does.
    // Opt to follow Blink's behavior (see bug 687787).
    if (mEventMessage == eFocusOut || originalWindowFocus == mOriginalFocusedContent) {
      InternalFocusEvent event(true, mEventMessage);
      event.mFlags.mBubbles = true;
      event.mFlags.mCancelable = false;
      event.mRelatedTarget = mRelatedTarget;
      return EventDispatcher::Dispatch(mTarget, mContext, &event);
    }
    return NS_OK;
  }

  nsCOMPtr<nsISupports> mTarget;
  RefPtr<nsPresContext> mContext;
private:
  EventMessage mEventMessage;
  nsCOMPtr<nsPIDOMWindowOuter> mOriginalFocusedWindow;
  nsCOMPtr<nsIContent> mOriginalFocusedContent;
  nsCOMPtr<EventTarget> mRelatedTarget;
};
```
### Long line wrapping
If statements on a single line become overly long, they should be split into multiple lines:
- Binary operators (including ternary) must be left on their original lines if the line break happens around the operator. The second line should start in the same column as the start of the expression in the first line.
- Lists of variables (e.g. when calling or declaring a function) should be split so variables are listed one-per-line, where second and subsequent lines start in the same column as the first variable listed, even if more than one variable would fit on the line segment until the wrapping column.

WRONG:
```C++
somelongnamespace::somelongfunction(var1, var2, var3,
                                    var4, var5);
somelongnamespace::somelongfunction(
  var1, var2, var3, var4, var5);
if (somelongvariable == somelongothervariable
  || somelongvariable2 == somelongothervariable2) {
somelongvariable = somelongexpression ? somevalue1
  : somevalue2;
```
CORRECT:
```C++
somelongnamespace::somelongfunction(var1,
                                    var2,
                                    var3,
                                    var4,
                                    var5);
if (somelongvariable == somelongothervariable ||
    somelongvariable2 == somelongothervariable2) {
somelongvariable = somelongexpression ?
                   somevalue1 :
                   somevalue2;
```
 
## JavaScript
Applies to `*.js` and `*.jsm`.
## XUL and other XML-derivatives 
Applies to `*.xul`, `*.html`, `*.xhtml`.
## IDL
Applies to `*.idl`, `*.xpidl` and `*.webidl`.

<!--stackedit_data:
eyJoaXN0b3J5IjpbMjUzODYzMzMsMjg5MjUwODIsLTQzMjMzNz
ExMSw4MzQ2MjYwNDksLTE5MDMyNzE5OTYsLTEwMTIwMjc3ODMs
LTE4MzgzODM5MDIsODA5MjEzNTEyLC01Mzg0MjM4MDAsMzgyNj
I3NDYzLDIwODYwMjIwODUsLTE1MjU5MjE2MjIsLTY1OTMzMTA0
MCwtNzQwOTE5MDQ1LDE4Njc1NTQxNDJdfQ==
-->