# SuaScript

SuaScript is a small, typed scripting language built into SuaEdit. Save scripts
with the `.sua` extension, open them in SuaEdit, and choose **Run** or press
**F5**. SuaEdit compiles the current editor buffer to bytecode and runs it in
the integrated Terminal. Run and F5 only compile files whose active filename
ends in `.sua`; text files are not sent to the compiler. Changes in an open
`.sua` buffer can be run without saving first.

## Types and Variables

Every variable declaration names its type and initializes its value:

```text
int count = 3;
bool enabled = true;
string message = "Hello from SuaScript";
```

The supported types are `int`, `bool`, and `string`. Variables have one scope
for the whole script and cannot be declared twice. There are at most 32
variables. SuaEdit can store up to 65,535 bytes in its editor buffer; the
compiler's token and instruction limits still apply to scripts.

## Output and Expressions

`print(value)` writes a value and a newline to the integrated Terminal:

```text
int answer = 6 * 7;
print("The answer is:");
print(answer);
```

Integer expressions support `+`, `-`, `*`, `/`, `%` (remainder), unary `-`,
and parentheses. Integer variables also support `+=`, `-=`, `*=`, `/=`, and
`%=` compound assignments.
Comparisons support `==`, `!=`, `<`, `<=`, `>`, and `>=`. Boolean expressions
support `and`, `or`, and `not`. Strings use double quotes; `\n` and `\t` are
supported in strings printed with `print`.

## Conditions and Loops

Blocks use braces. Conditions must have type `bool`:

```text
int score = 12;

if (score >= 20) {
    print("high score");
} else if (score >= 10) {
    print("good score");
} else {
    print("keep trying");
}

int remaining = 3;
while (remaining > 0) {
    print(remaining);
    remaining = remaining - 1;
}

for (int index = 0; index < 3; index = index + 1) {
    print(index);
}
```

The `for` initializer declares a typed variable; the condition and increment
are separated by semicolons. `break` exits the nearest loop and `continue`
starts its next iteration. `while` and `for` stop with a runtime error if a
script exceeds the instruction limit.

## Comments and Errors

Lines may contain `#` or `//` comments. Statements may end with a semicolon;
semicolons are required between the three parts of a `for` header. Syntax and
type errors are reported in the Terminal with a source line number. Division
by zero and scripts that exceed the runtime instruction limit also report an
error instead of hanging SuaEdit.