# Godot Lua Nob

**Godot Lua Nob** ist ein produktionsorientiertes Starter-Repository für ein performance-fokussiertes **LuaJIT-Binding für Godot 4**. Das Projekt verwendet **GDExtension**, bindet LuaJIT statisch als PIC-Bibliothek ein und nutzt **nob.h** als primäres, selbsthostendes Build-System. Die Architektur ist auf große Spielprojekte ausgelegt: keine Engine-Fork, kein fragiler Vollwrapper für jede Godot-Klasse, sondern eine wartbare dynamische Brücke über `Variant`, `Object`, Singletons, Callables, Signals und Godot-Methoden.

> Godot 4 GDExtension ist die moderne native Erweiterungsschnittstelle für Godot, bei der externe Shared Libraries zur Laufzeit geladen werden. Für langfristige Projekte ist dieser Ansatz meist wartbarer als ein eigener Engine-Fork, weil das Spiel mit offiziellen Godot-Releases aktualisiert werden kann.[1] Die C++-Bindings werden über `godot-cpp` bereitgestellt und aus der passenden `extension_api.json` generiert.[2]

## Architekturentscheidung

Für ein sehr großes, performance-kritisches Spielprojekt ist die beste Basis **Godot 4 + GDExtension + LuaJIT + nob.h**. LuaJIT ist nicht Lua 5.4, sondern primär Lua-5.1-kompatibel mit JIT, FFI und eigenen Erweiterungen.[4] Diese Wahl ist bewusst: Für Gameplay, Modding-ähnliche Systeme und datengetriebene Runtime-Logik ist die Laufzeitperformance wichtiger als die exakte Lua-5.4-Sprachsemantik. Sicherheits- und Portabilitätsrisiken werden über ein Policy-Modell abgefangen.

| Bereich | Entscheidung | Begründung |
|---|---:|---|
| Godot-Integration | **GDExtension** | Native Performance ohne Engine-Fork und mit klarer Godot-4-Kompatibilität.[1] |
| C++-Binding | **godot-cpp** | Offizielle C++-Schicht über der GDExtension-C-API.[2] |
| Lua-Runtime | **LuaJIT** | Hohe Ausführungsgeschwindigkeit, JIT-Control und optionales FFI für vertrauenswürdige Tooling-/Engine-Skripte.[4] |
| Build-System | **nob.h** | Selbsthostender C-Build-Runner, der im Repository liegt und keine Generator-Magie versteckt.[3] |
| Godot-API-Modell | **Dynamische Variant/Object-Brücke** | Besser wartbar als tausende handgeschriebene Lua-Wrapper. |
| Projektstatus | **Erweiterte Foundation** | Enthält LuaJIT-Build, Runtime-Policies, `require`, Variant/Object-Brücke, Callables/Signals, Demo und CI-Vorlage. |

## Repository-Struktur

Das Repository ist so aufgebaut, dass Gameplay-Teams die Runtime direkt ausprobieren und Engine-/Tools-Teams sie schrittweise erweitern können. `nob.c` ist der zentrale Build-Runner; `tools/generate_godot_cpp.py` erzeugt die fehlenden godot-cpp-Klassenbindungen direkt aus `extension_api.json`, ohne SCons zu benötigen.

| Pfad | Zweck |
|---|---|
| `nob.c` | Primäres Build-System mit LuaJIT-Build, godot-cpp-Codegenerierung, statischen Libraries und finaler GDExtension. |
| `include/godot_lua/` | Öffentliche C++-Header für Runtime, Fehlerobjekte, Callback-Klasse, Variant-Brücke und ScriptLanguageExtension-Schicht. |
| `src/runtime/` | `LuaState`, `LuaError` und `LuaCallable` als Godot-registrierbare Klassen. |
| `src/bindings/` | Konvertierung zwischen Lua-Werten und Godot-`Variant`, Objektzugriff, Singletons, `require` und Utility-Funktionen. |
| `src/script/` | `LuaScriptLanguage`, `LuaScript` und `LuaScriptInstance` als Grundlage für editornahe `.lua`-Skripte und Lifecycle-Dispatch. |
| `thirdparty/luajit/` | Eingebetteter LuaJIT-Quellcode. |
| `thirdparty/godot-cpp/` | Offizielle Godot-C++-Bindings. Generierte Dateien liegen unter `thirdparty/godot-cpp/gen/` und werden nicht versioniert. |
| `demo/` | Minimales Godot-Projekt mit `.gdextension`, Szene, GDScript, LuaJIT-Skript und Lua-Modulbeispiel. |
| `docs/ci/github-actions-build.yml` | CI-Vorlage; wegen Token-Berechtigungen nicht direkt unter `.github/workflows/` abgelegt. |

## Build

Der Build ist absichtlich schlicht. Zunächst wird der Build-Runner aus `nob.c` kompiliert, anschließend übernimmt `./nob` die vollständige Pipeline. Auf einem typischen Linux-Entwicklungsrechner werden ein C-Compiler, ein C++17-Compiler, `make`, `ar` und Python 3 benötigt.

```bash
git clone --recursive https://github.com/Mankimann/godot-lua-nob.git
cd godot-lua-nob
cc -o nob nob.c
./nob build debug platform=linux arch=x86_64
```

Für Release-Builds wird der Modus entsprechend geändert.

```bash
./nob build release platform=linux arch=x86_64
```

Die erzeugte Shared Library landet unter `demo/addons/godot_lua/bin/`. Die `.gdextension`-Datei im Demo-Projekt zeigt bereits auf die üblichen Debug- und Release-Pfade. Wenn ein anderes Toolchain-Setup verwendet werden soll, können `CC`, `CXX`, `AR` und LuaJIT-Make-Variablen über die Umgebung überschrieben werden.

```bash
CC=clang CXX=clang++ AR=llvm-ar ./nob build release
```

## Nutzung in Godot

Im aktuellen Modell erzeugt GDScript eine `LuaState`-Instanz, wählt eine Runtime-Policy, fügt Modulpfade hinzu und führt Lua-Dateien aus. Das ist absichtlich explizit, weil große Projekte meist mehrere Lua-States mit unterschiedlichen Vertrauenszonen benötigen.

```gdscript
var lua := LuaState.new()
lua.open_with_policy(LuaState.POLICY_GAMEPLAY)
lua.add_module_root("res://scripts")
lua.jit_optimize("hotloop=56,hotexit=10")
lua.set_global("godot_node", self)
lua.do_file("res://scripts/hello.lua")
```

## Native Lua-Skriptintegration

Zusätzlich zur expliziten `LuaState`-Nutzung registriert die GDExtension nun eine **ScriptLanguageExtension** für die Dateiendung `.lua`. Die erste Ausbaustufe besteht aus drei Bausteinen: `LuaScriptLanguage` meldet die Sprache im Engine-Subsystem an, `LuaScript` verwaltet Lua-Quellcode als Godot-`Script`-Resource, und `LuaScriptInstance` führt Lifecycle-Funktionen wie `_ready`, `_process`, `_physics_process`, `_input` und `_unhandled_input` kontrolliert über eine LuaJIT-State-Instanz aus.

| Klasse | Aufgabe | Status |
|---|---|---|
| `LuaScriptLanguage` | Sprachmetadaten, `.lua`-Erkennung, Templates, Syntaxvalidierung per LuaJIT-Parser und Engine-Registrierung. | Implementiert als konservative `ScriptLanguageExtension`. |
| `LuaScript` | Speichert Quellcode, erkennt Lua-Funktionen, liefert Editor-/Reflection-Metadaten und Script-Resource-Verhalten. | Implementiert; native C-API-`ScriptInstance`-Handle ist als nächster ABI-Schritt markiert. |
| `LuaScriptInstance` | Führt ein Lua-Skript mit `self`-Owner aus und dispatcht Lifecycle-Methoden explizit aus Godot. | Nutzbar aus GDScript und C++ als sicherer Übergangspfad. |

Das Demo-Projekt enthält `demo/scripts/native_node.lua` und zeigt in `demo/scripts/main.gd`, wie eine `LuaScript`-Resource erzeugt, mit Quellcode gefüllt und über `LuaScriptInstance` ausgeführt wird. Damit ist die Sprachregistrierung im Editor vorbereitet, während der finale Godot-C-API-`ScriptInstance`-Descriptor separat und risikoarm ergänzt werden kann.

## LuaJIT-API

Die Runtime stellt eine globale Tabelle `godot` bereit. Godot-Objekte werden in Lua als Userdata gehalten. Methodenaufrufe erfolgen dynamisch über Godots `Object::callv`; Properties werden über `Object::get` und `Object::set` angebunden. Dadurch kann dieselbe Brücke mit beliebigen Godot-Klassen arbeiten, ohne für jede Klasse eine separate Lua-Datei oder C++-Binding-Datei zu generieren.

| Lua-Funktion | Beschreibung |
|---|---|
| `godot.print(...)` | Schreibt über Godots `UtilityFunctions::print` in die Godot-Konsole. |
| `godot.push_warning(text)` | Schreibt eine Godot-Warnung. |
| `godot.push_error(text)` | Schreibt einen Godot-Fehler. |
| `godot.get_singleton(name)` | Gibt ein Godot-Singleton wie `Engine`, `Input` oder `ProjectSettings` als Lua-Objekt zurück. |
| `godot.load_resource(path, type_hint?)` | Lädt eine Godot-Resource über `ResourceLoader`. |
| `godot.variant_type(value)` | Liefert den Godot-Variant-Typnamen eines Lua-Werts. |
| `godot.is_instance_valid(object)` | Prüft, ob ein Godot-Objekt noch gültig ist. |
| `godot.callable(fn)` | Wandelt eine Lua-Funktion in ein Godot-`LuaCallable`-Objekt um. |
| `godot.connect(object, signal_name, fn)` | Verbindet ein Godot-Signal mit einer Lua-Funktion und hält den Callback am Leben. |
| `Vector2(x, y)` | Erzeugt einen Godot-`Vector2`. |
| `Vector3(x, y, z)` | Erzeugt einen Godot-`Vector3`. |
| `Vector4(x, y, z, w)` | Erzeugt einen Godot-`Vector4`. |
| `Color(r, g, b, a)` | Erzeugt eine Godot-`Color`. |

## Module und `require`

`LuaState.add_module_root("res://scripts")` registriert einen Godot-Dateisystempfad für Lua-Module. Danach kann LuaJIT Module mit Punktnotation laden. Der Loader sucht nach `<root>/<modul>.lua` und `<root>/<modul>/init.lua`.

```lua
local math_util = require("game.math_util")
godot.print(math_util.add(20, 22))
```

## Runtime-Policies

Die Runtime unterscheidet mehrere Vertrauenszonen. Für ein großes Projekt ist das wichtiger als ein globales Alles-oder-Nichts-Sandboxing, weil interne Engine-Tools andere Rechte brauchen als Modding- oder Live-Ops-Skripte.

| Policy | Ziel | JIT | FFI | Bytecode | Dateisystem/Native Loadlib |
|---|---|---:|---:|---:|---|
| `POLICY_TRUSTED` | Interne Engine-/Tooling-Skripte | An | Optional | Optional | Kann bewusst freigegeben werden. |
| `POLICY_GAMEPLAY` | Normale Gameplay-Skripte | An | Aus | Aus | Godot-Loader für `res://`/`user://`, kein natives `loadlib`. |
| `POLICY_SANDBOXED` | Untrusted/Modding-nahe Skripte | Optional | Aus | Aus | Entfernt riskante `os`, `io`, `loadfile`, `dofile`-Wege. |
| `POLICY_MODDING` | Späterer Modding-Ausbau | Projektabhängig | Aus | Aus | Sollte zusätzlich Ressourcenlimits und Pfad-Whitelists bekommen. |

> Die Sandbox ist ein **Sicherheitsfundament**, kein endgültiges Sicherheitsmodell. Für Modding in Produktion müssen Pfad-Policies, Ressourcenlimits, deterministische Timeouts, Speicherbudgets und gegebenenfalls getrennte Lua-States pro Vertrauenszone ergänzt werden.

## Demo

Das Demo-Projekt liegt in `demo/`. Nach einem erfolgreichen Build kann der Ordner in Godot 4 geöffnet werden. Die Szene `demo/scenes/main.tscn` startet `demo/scripts/main.gd`, erzeugt zuerst eine `LuaState`-Instanz und führt `demo/scripts/hello.lua` aus. Anschließend lädt die Demo `demo/scripts/native_node.lua` als `LuaScript`, initialisiert eine `LuaScriptInstance`, ruft `_ready` sowie eine Lua-Methode auf und gibt die erkannten Lua-Methoden aus. Das Beispiel demonstriert LuaJIT, `require`, Godot-Singletons, Variant-Konvertierung, globale Funktionsaufrufe aus GDScript, Lua-Funktionen als Signal-Callbacks und die neue Script-Resource-Schicht.

## Typed Lua-API-Generator

Für große Projekte enthält `tools/generate_api_wrappers.py` einen Generator für typisierte Lua-Stubs. Er liest eine Godot-`extension_api.json`, erzeugt EmmyLua/LuaLS-kompatible Klassenmodule und kann zusätzlich ein Projektverzeichnis nach `class_name`-GDScript-Dateien sowie nach Lua-Dateien mit `---@class`-Annotationen scannen.

```bash
python3.11 tools/generate_api_wrappers.py \
  --api /path/to/extension_api.json \
  --project demo \
  --out generated/lua_api
```

Die generierten Dateien sind bewusst Editor-/Tooling-Stubs. Zur Laufzeit bleibt die dynamische `Variant`/`Object`-Brücke maßgeblich, sodass stabile Projekt-APIs typisiert werden können, ohne den nativen Binding-Code für jede Klasse neu zu kompilieren.

## Roadmap für ein nahezu vollständiges Binding

Dieses Repository ist nun deutlich über ein MVP hinaus ausgebaut und enthält eine erste `ScriptLanguageExtension`-Schicht. Der nächste große Schritt zu einem vollständig Editor-nativen Lua-Erlebnis ist der niedrige Godot-C-API-`ScriptInstance`-Descriptor, damit `.lua`-Dateien ohne expliziten Wrapper an Nodes hängen und direkt am Engine-Lifecycle teilnehmen.

| Priorität | Erweiterung | Nutzen |
|---:|---|---|
| 1 | Native C-API-`ScriptInstance`-Descriptor | Vollautomatisches Anhängen von `.lua`-Dateien an Nodes mit Engine-Lifecycle ohne GDScript-Wrapper. |
| 2 | Projektinterner typed wrapper generator | Schnellere, typisierte Lua-Wrapper für stabile eigene Gameplay-APIs; Grundversion vorhanden. |
| 3 | Ressourcenlimits und Timeouts | Sichere Produktion für Modding- oder Live-Scripting. |
| 4 | Headless-Godot-Integrationstests | CI-validierte Laufzeitintegration. |
| 5 | Editor-Diagnostics und Debugger-Adapter | Besserer Workflow für große Teams. |

## Lizenzhinweis

Dieses Repository enthält Third-Party-Quellen. Vor einem öffentlichen Release müssen die jeweiligen Lizenzdateien von LuaJIT, godot-cpp und nob.h geprüft und vollständig beibehalten werden. Für dein privates Projekt ist der aktuelle Aufbau geeignet; für Distributionen sollte zusätzlich `THIRD_PARTY_NOTICES.md` gepflegt bleiben.

## References

[1]: https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html "Godot Documentation: GDExtension"
[2]: https://github.com/godotengine/godot-cpp "godotengine/godot-cpp"
[3]: https://github.com/tsoding/nob.h "tsoding/nob.h"
[4]: https://luajit.org/ "LuaJIT"
[5]: https://luajit.org/install.html "LuaJIT Installation"
