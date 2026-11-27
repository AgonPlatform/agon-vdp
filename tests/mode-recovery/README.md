# MODE recovery checks

The host fixture substitutes a drawing-pool allocator and checks the MODE
handler on the patched source. It exercises successful MODE 0 transitions
and a MODE 1 fallback after a refused allocation.

From the Bistromathics checkout, set `graphics` to a vdp-gl checkout at
the commit in `platformio.ini`, then run:

```sh
out=$(mktemp -d)
python3 tests/mode-recovery/check_mode_recovery.py . "$out/mode-bodies.inc" --graphics "$graphics"
c++ -std=c++17 -fsanitize=address,undefined -I "$out" tests/mode-recovery/check_mode_recovery.cpp -o "$out/check"
"$out/check"
```
