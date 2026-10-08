# Translating the original game

This engine is the original Desperados program, translated function by function from its
decompiled Linux build (`desperados32`, the 2018 THQ Nordic port, which still carries every
class and method name). The goal is the original behaviour, exactly: same rules, same numbers,
same order of operations, same quirks. Only the platform layer (window, OpenGL, files, sound,
input) is our own, so the game can run natively on Android with touch controls.

## Where things are

| Path | What |
|---|---|
| `src/platform/` | our own platform layer, standing in for the port's `thqsdlfw_*`, `RFILE_*`, `RMIX_*` functions (same names, same behaviour) |
| `src/sb/` | the original's "SBLib" (Spellbound library): files, pictures, drawing, geometry, UI widgets, fonts |
| `src/dv/` | the game itself ("Death Valley"): engine, elements, AI, scripts glue, menus |
| `src/vm/` | the mission-script virtual machine (`VMCore`, `SC*`) |
| `src/app/` | our app: startup, touch controls |
| `tools/newclass.py` | creates the header/source skeleton of a class from the recovered layout |

Reverse-engineering data (not in the repository, it is derived from the game binary) lives in
`/home/claude/decomp`:

| File | What |
|---|---|
| `c/<Class>.c` | decompiled C of every function of a class (Ghidra), in address order. `c/_global.c` has the free functions |
| `c2/<Class>.c` | the same with partly recovered struct fields (`this->fieldN_0x48`); many functions failed there, use it only as a hint |
| `show.sh <Class> <Class::Method>` | prints one function without its local declarations |
| `vtables.txt` | every vtable: `+0x14 DVElement::IsTransporting() const` = the virtual call `(*(code **)(*p + 0x14))(p)` |
| `hierarchy.json` | base classes (from RTTI) |
| `layout.json` | per class: recovered size, embedded objects, accessed fields with the types seen |
| `syms.txt` | every symbol (methods, static members, globals) |
| `desperados32` | the binary; `objdump -d --start-address=0x... desperados32` when the decompiler looks wrong |

## Rules

1. **One class = one `.h` + `.cpp`**, named like the original (`sb/SBDrawManager.h`). Create the
   skeleton with `python3 tools/newclass.py ClassName`. It declares every method and defines each as
   a stub (`STUB("Class::Method")`, logs once at runtime). Translating a method = replacing its stub
   body. Keep the `// <address>` comment above every function: it is how we track progress.
2. **Fields keep their original offset in their name**: `f_b4c` is the field at `this + 0xb4c` in the
   original 32-bit object, so anyone translating `*(float *)(engine + 0xb4c)` writes `engine->f_b4c`
   without having to coordinate. Give the real type (pointer, float, int16_t...). You may add a
   meaningful suffix once you know what it is (`f_b4c_cameraX`), but then fix every use (grep).
   Embedded objects are fields too (`DVFrameHolder f_168;`), and an access to `this + 0x16c` inside
   it is `f_168.f_4`. Base classes are real C++ bases.
3. **Keep the original integer widths and signedness** (`uint16_t`, `int16_t`, `uint8_t`...):
   wrap-around and truncation are part of the behaviour. Floats stay `float` unless the original
   used `double`. The build uses `-fwrapv` (signed overflow wraps like on x86).
4. **Translate, don't redesign.** Same control flow, same constants, same call order, same side
   effects. If the original looks buggy, keep it and mark it `// (sic)`. Readability is welcome
   (names for locals, `for` loops, comments), changes of behaviour are not.
5. **Inlined library code becomes the library call.** The decompiler shows inlined standard
   containers as raw pointer walks:
   * `std::string`: `{char* p; size_t len; char buf[16]}` (24 bytes); `_M_create`, `_M_append`,
     `_M_assign`, `_M_replace`, `compare` = ordinary `std::string` operations. Strings built from
     integer constants (`local_2c = 0x61746144` ...) are text: decode the bytes (little endian).
   * `std::map` / `std::set` (`_Rb_tree`, header 24 bytes: `+4 root, +8 leftmost, +0xc rightmost,
     +0x10 count`; nodes: `+0x10` key, then value): the typical "walk the tree comparing with
     `+0x10`" loop is `map.find(key)` / `lower_bound`.
   * `std::list` (`_List_node_base`: next, prev, then the value) and `std::vector` (begin, end,
     capacity).
   * `SBList<T>` (24 bytes: std::list + count at +8, cache node +0xc, cache-invalid flag +0x10,
     cache index +0x14) and `SBArray<T>` (std::vector + 2 fields) are in `sb/SBContainers.h`. The long
     "walk from the front, the back or the cached node" code is just `list[i]`.
6. **Virtual calls**: `(**(code **)(*(int *)p + 0x2c))(p, ...)` = the vtable slot `+0x2c` of p's
   class, look it up in `vtables.txt`, write `p->Method(...)`. Methods in a vtable are `virtual` in
   the header (the generator does this).
7. **Platform calls** keep their names: `thqsdlfw_set_tech`, `thqsdlfw_blit`, `RFILE_read`... are in
   `platform/thq.h`. Missing ones: add them there (our implementation), following the original's
   behaviour (`c/_global.c` has the port's own code).
8. **Calling something not translated yet**: if the class has no files yet, create them with
   `newclass.py`; the method then exists as a stub. Translate it too when it matters for your task,
   otherwise leave the stub.
9. **Decompiler traps**: `CONCAT44`, `SUB41`, `_4_4_` are byte packing; `(float)((double)(x |
   0x4330000000000000) - 4503599627370496.0)` is `(float)(uint32_t)x`; `-(uint)(a < b) & x` is a
   branch-free select; `(int)(f - 2.1474836e+09) ^ 0x80000000` is the float to unsigned conversion.
   Ghidra sometimes drops `this` or merges it into a parameter (`_param_1`), invents wrong types,
   or loses a `return`; when in doubt read the assembly (`objdump -d --no-show-raw-insn
   --start-address=0xADDR --stop-address=0xEND desperados32`).
10. **Build often**: `cmake -S engine -B build-<yourname> -DCMAKE_BUILD_TYPE=Debug && cmake --build
    build-<yourname> -j2`. The build must stay green: fix what you broke, never leave a file that
    does not compile.

## Conventions

* `this` pointer arithmetic on fields becomes member access; `param_1` becomes a named parameter.
* Globals and static members keep their original names (`DVEngine::mpEngine`, `gGlobalOptions`).
* Strings in error messages and file names stay byte-identical.
* Do not add includes of other classes' `.cpp` or circular header includes; forward-declare.
