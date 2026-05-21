# Architektur: Godot Lua Nob

Dieses Dokument beschreibt die Zielarchitektur des Repositories. Der Kernentscheid lautet: **Godot 4 GDExtension, LuaJIT, dynamische Variant/Object-Brücke und nob.h als primäres Build-System**. Das Ziel ist ein performantes, wartbares und kontrollierbares Lua-System für sehr große Godot-Projekte, ohne einen Godot-Engine-Fork pflegen zu müssen.

## Entscheidung

LuaJIT wird als primäre Runtime verwendet, weil das Projekt ausdrücklich performance-orientierte Gameplay- und Tooling-Skripte unterstützen soll. Die Entscheidung bedeutet bewusst, dass die Sprache nicht Lua 5.4 ist, sondern LuaJIT-kompatibles Lua auf Basis der Lua-5.1-Semantik mit JIT, FFI und LuaJIT-Erweiterungen. Für große Produktionen ist diese Klarheit wichtig, damit Teams keine Lua-5.4-only-Features in Gameplay-Code voraussetzen.

| Ebene | Wahl | Begründung |
|---|---|---|
| Native Integration | Godot 4 GDExtension | Native Erweiterung ohne Engine-Fork und mit offiziellen Godot-4-Bindings. |
| C++-Schicht | godot-cpp | Offizielle C++-Abstraktion über der GDExtension-C-API. |
| Skript-Runtime | LuaJIT | Hohe Performance, steuerbarer JIT und optionales FFI für vertrauenswürdige Skripte. |
| Build | nob.h | Einfacher, versionierter und selbsthostender Build-Runner. |
| API-Modell | Dynamische Brücke | Wartbarer als eine vollständig statisch generierte Lua-Spiegelung aller Godot-Klassen. |

## Laufzeitmodell

`LuaState` ist die zentrale Godot-registrierbare Klasse. Sie besitzt einen eigenen `lua_State`, öffnet LuaJIT-Bibliotheken, installiert Godot-spezifische Loader und wendet ein Runtime-Policy-Modell an. Dadurch können große Projekte mehrere Lua-States mit unterschiedlichen Rechten betreiben, beispielsweise für Gameplay, interne Tools oder Modding-nahe Skripte.

| Komponente | Verantwortung |
|---|---|
| `LuaState` | Besitz des `lua_State`, Ausführung von Dateien/Strings, globale Variablen, JIT-Control, Diagnostics. |
| `LuaError` | Godot-seitig transportierbares Fehlerobjekt für geschützte Lua-Aufrufe. |
| `LuaCallable` | Brücke von Lua-Funktionen zu Godot-Callables und Signal-Callbacks. |
| `lua_variant_bridge` | Konvertierung zwischen Lua-Werten und Godot-`Variant`, Object-Userdata, Module Loader und globale `godot`-API. |
| `LuaScriptLanguage` | Godot-Sprachregistrierung für `.lua`, Templates, Syntaxvalidierung und Editor-Reflection-Stubs. |
| `LuaScript` | Godot-`Script`-Resource für Lua-Quellcode, Datei-Loading, Methodenerkennung, Metadaten und native ScriptInstance-Erzeugung. |
| `LuaScriptInstance` | Runtime-Wrapper und interne Nutzlast für Owner-Objekt, LuaState, Initial-Properties, Reload, Diagnostik und Lifecycle-Dispatch. |
| `LuaScriptHost` | GDScript-Node-Brücke als robuster Fallback und Inspector-freundlicher Host für Lua-Komponenten. |
| `godot_lua_editor` | Demo-Editor-Plugin für `.lua`-Import, Inspector-nahen Lua-Script-Slot und Menü-basierten Attach an ausgewählte Nodes. |

## Build-Pipeline

Die Build-Pipeline wird vollständig von `nob.c` gesteuert. LuaJIT wird als statische PIC-Bibliothek gebaut, godot-cpp wird aus der API-Beschreibung generiert und die finale GDExtension wird als Shared Library erzeugt.

```text
nob.c
  ├─ build LuaJIT static PIC library
  ├─ generate godot-cpp bindings from extension_api.json
  ├─ compile godot-cpp sources
  ├─ compile godot-lua runtime and bindings
  └─ link shared GDExtension library into demo/addons/godot_lua/bin
```

## ScriptLanguageExtension-Schicht

Die erste editornahe Schicht registriert `LuaScriptLanguage` über `Engine::register_script_language()` als Godot-Skriptsprache für die Endung `.lua`. `LuaScript` speichert und analysiert Quellcode als `ScriptExtension`-Resource, kann Dateien direkt per `load_from_file(path)` laden und erzeugt über die Godot-C-API einen nativen `GDExtensionScriptInstance`-Descriptor. `LuaScriptInstance` kapselt die kontrollierte Ausführung gegen ein Besitzerobjekt, setzt `self` und `owner`, übernimmt Initial-Properties, leitet den Skriptordner als Modul-Root ab und ruft Lifecycle-Funktionen wie `_ready`, `_process`, `_physics_process`, `_input`, `_unhandled_input` und `_exit_tree` über die bestehende `LuaState`-API auf. Beim direkten `Node.set_script(LuaScript)` verwendet der native Descriptor diesen Wrapper intern als private Nutzlast. Das Demo-Editor-Plugin `godot_lua_editor` ergänzt diesen Pfad um einen `.lua`-Importer, eine UndoRedo-fähige Menüaktion für ausgewählte Nodes und einen Inspector-nahen **Lua Script**-Slot mit Browse-, Attach-, Reload- und Clear-Aktionen.

| Ebene | Umsetzung | Produktionsnotiz |
|---|---|---|
| Sprachregistrierung | `LuaScriptLanguage : ScriptLanguageExtension` | Singleton-Lebensdauer wird in `register_types.cpp` gehalten, weil Godot einen stabilen Sprachzeiger speichert. |
| Script-Resource | `LuaScript : ScriptExtension` | Quellcode, Pfadhinweis, Datei-Loading, einfache Funktionsanalyse, Reflection-Listen und `_instance_create()` mit `GDExtensionScriptInstanceInfo3` sind implementiert. |
| Runtime-Instanz | `LuaScriptInstance : RefCounted` | Expliziter Dispatch-, Reload- und Diagnostikpfad; zusätzlich interne Nutzlast des nativen ScriptInstance-Descriptors. |
| Host-Brücke | `demo/scripts/lua_script_host.gd` | Wiederverwendbarer Node-Wrapper als robuster Fallback, wenn ein Projekt Lua bewusst über einen GDScript-Host steuern will. |
| Editor-Plugin | `demo/addons/godot_lua_editor` | Importiert `.lua` als `LuaScript`, ergänzt Node-Inspektoren um einen Lua-Script-Slot und hängt ausgewählte Nodes über den nativen `set_script`-Pfad an. |
| Typisierte Tooling-API | `tools/generate_api_wrappers.py` | Erzeugt EmmyLua/LuaLS-Stubs aus `extension_api.json`, Projektklassen und Runtime-Helfern, ohne die Runtime-Brücke zu ersetzen. |

## Godot-Brücke

Die Brücke setzt bewusst auf Godots dynamische `Variant`- und `Object`-APIs. Lua-Tabellen werden je nach Form zu Arrays, Dictionaries, Vektoren oder Farben konvertiert. Godot-Objekte werden als Userdata gespeichert, wobei die Runtime nur die `ObjectID` hält und bei Zugriffen über `ObjectDB` prüft, ob die Instanz noch lebt.

| Richtung | Verhalten |
|---|---|
| Godot zu Lua | Nil, Bool, Int, Float, String, Vector2/3/4, Color, PackedByteArray, Array, Dictionary und Object werden nach Lua übertragen. |
| Lua zu Godot | Nil, Bool, Number, String, Tables und Godot-Object-Userdata werden als `Variant` gelesen. |
| Objektzugriff | Lua `obj.foo` liest Godot-Properties; `obj.foo = value` schreibt sie. |
| Methoden | Lua `obj:method(args...)` ruft dynamisch Godot-Methoden über `callv`. |
| Signale | `godot.connect(object, signal, fn)` erstellt einen `LuaCallable` und verbindet ihn mit dem Godot-Signal. |

## Sicherheit

Die Runtime verwendet Policies statt einer einzigen globalen Sandbox. Das ist für große Produktionen entscheidend, weil unterschiedliche Skriptklassen unterschiedliche Rechte benötigen. In Gameplay- und Sandbox-Modi werden gefährliche globale Funktionen, native Loader und FFI standardmäßig deaktiviert. FFI sollte nur für vertrauenswürdige interne Skripte aktiviert werden.

> LuaJIT-FFI ist mächtig und kann native Grenzen umgehen. In einem Spielprojekt sollte FFI als Engine-/Tooling-Funktion behandelt werden, nicht als Standardfähigkeit für Gameplay- oder Modding-Code.

## Nächste Architektur-Stufe

Die aktuelle Architektur ist eine erweiterte Runtime-Foundation mit `ScriptLanguageExtension`-Integration, nativem Node-Attach, Inspector-nahem Demo-Editor-Plugin, praktischer Host-Brücke und Typed-API-Generator. Für ein nahezu vollständiges Godot-Lua-Erlebnis sind die nächsten Schritte Integration in Godots originalen Script-Resource-Picker, Headless-Godot-Integrationstests, Ressourcenlimits und ein Debugger-/Diagnostics-Workflow im Editor.
