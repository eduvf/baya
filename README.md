# 🫐 baya fantasy console

## specifications

* 64x32 screen resolution
* 8 color palette
* 12 fps
* 8x4 sprites
* 5 buttons (up/w, down/s, left/a, right/d, space/enter)
* 10 general-purpose registers (`x y z w a b c d e f`)
* 1 time register (`t`): increases by 1 every frame
* 1 internal index pointer (`ip`)
* 4kB memory

## language reference

preprocessed instructions (used only in compilation):

* `alias x name` bind a name to a register
* `write N` write a byte directly to memory
* `: L` create a label L

instructions:

* `goto L` jump execution to label L
* `point L` move the `ip` to the label L

* `x = N` assign a literal to a register
* `x = y` assign a register to a register
* `x += N` add a literal to a register
* `x += y` add a register to a register (also `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`)
* `x = random N` assign a random number between 0 (inclusive) and N (exclusive)
* `print x` print the value of a register (to console)

* `if x == N then` only execute the next instruction if register x equals literal N
* `if x != N then` only execute the next instruction if register x does not equal literal N
* `if x == y then` only execute the next instruction if register x equals register y (also `!=`, `<`, `<=`, `>`, `>=`)
* `key action then` only execute the next instruction if button action is pressed (action is space or enter, and up/left/down/right is the arrow keys or w/a/s/d)

* `clear` clear the screen
* `sprite x y c` draws a sprite from the `ip` at coordinates x y and color c

* `save` store the state of all registers in the stack
* `load` restore the state of all registers from the stack
