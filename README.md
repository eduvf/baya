# 🫐 baya fantasy console

## specifications

* 11 registers (x y z w a b c d e f t)
* 16 sprites (8x4 pixels)
* 4kB memory

## language reference

```
( comment )

x = 255

x += 1

x = random 10

x = y
x += y (also -= *= /= %= &= |= ^=)

: label

goto label

if x == 0 then ...
if x != 0 then ...
if x == y then ... (also != < <= > >=)
```
