# LuaJIT Migration Notes

Diese Notizen halten die technischen Entscheidungen für die Umstellung von Lua 5.4 auf **LuaJIT 2.1** fest.

LuaJIT ist laut offizieller Dokumentation vollständig aufwärtskompatibel zu **Lua 5.1** und ABI-kompatibel zur Lua-5.1-C-API. Es bietet zusätzliche Module wie `bit`, `ffi`, `jit` sowie partielle Lua-5.2- und Lua-5.3-Erweiterungen. Für das Binding bedeutet das: Der Fokus verschiebt sich von Lua 5.4 auf **LuaJIT-kompatibles Lua**, wobei Lua-5.4-only-Features nicht versprochen werden dürfen.

| Bereich | Entscheidung |
|---|---|
| Runtime | LuaJIT 2.1 als primäre eingebettete Runtime. |
| API-Kompatibilität | Lua-5.1-C-API plus LuaJIT-Erweiterungen. |
| Lua-5.2-Kompatibilität | Build optional mit `-DLUAJIT_ENABLE_LUA52COMPAT`. |
| FFI | Per Runtime-Policy steuerbar; für untrusted Scripts deaktivierbar/ausblendbar. |
| JIT | Per `jit.on/off/flush/opt.start` aus Godot steuerbar. |
| Sicherheit | `io`, `os`, `package.loadlib`, `ffi` und native Loader dürfen nicht pauschal für Mods offen sein. |
| Performance | Optimierte Build-Flags, JIT-Control, table preallocation (`table.new`) und klare Hot-Path-Empfehlungen. |

## Quellen

[1]: https://luajit.org/extensions.html "LuaJIT Extensions"
[2]: https://luajit.org/install.html "LuaJIT Installation"
[3]: https://github.com/LuaJIT/LuaJIT "LuaJIT GitHub Mirror"
