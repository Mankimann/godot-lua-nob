# Architektur: Lua-Binding für Godot 4

Dieses Dokument beschreibt die technische Zielarchitektur des Repositories. Der Kernentscheid lautet: **Godot 4 GDExtension, Lua 5.4, dynamische Variant/Object-Brücke und nob.h als primäres Build-System**.

## Zielsetzung

Das Binding soll für ein sehr großes Spielprojekt geeignet sein. Deshalb ist langfristige Wartbarkeit wichtiger als ein maximal breiter statischer Wrapper in der ersten Version. Godot wird nicht geforkt; die Runtime wird als externe GDExtension geladen. Lua wird statisch eingebettet, damit das Spiel eine kontrollierte und reproduzierbare Skriptumgebung erhält.

| Ziel | Konsequenz für das Design |
|---|---|
| Große Codebasis | Kleine, stabile C++-Kernschicht statt massiver generierter Lua-Wrapper. |
| Langlebige Godot-Versionen | Bindings werden aus Godots `extension_api.json` erzeugt. |
| Modding- und Gameplay-Scripting | Mehrere Lua-States und Sandbox-Policies sind vorgesehen. |
| CI- und Toolchain-Kontrolle | nob.h orchestriert Build-Schritte direkt und sichtbar. |
| Portabilität | Lua 5.4 statt LuaJIT als Standard. |

## Laufzeitmodell

`LuaState` kapselt einen `lua_State*` und wird als Godot-Klasse registriert. Jede Instanz kann isoliert konfiguriert werden. Für große Projekte ist dies besonders wichtig, weil Gameplay, Mods, Tools und Live-Daten unterschiedliche Vertrauens- und Performanceprofile haben.

Die dynamische Brücke konvertiert Lua-Werte nach Godot-`Variant` und zurück. Primitive Werte, Strings, Arrays, Dictionaries, Vektoren, Farben und Godot-Objekte sind im Fundament enthalten. Godot-Objekte werden als Lua-Userdata gespeichert, wobei nur die Godot-Instance-ID gehalten wird. Bei Zugriffen wird das Objekt über Godots ObjectDB erneut aufgelöst, damit gelöschte Objekte nicht direkt dereferenziert werden.

## API-Brücke

Die Brücke bevorzugt **dynamische Calls** über `Object::callv`. Dadurch kann Lua beliebige Godot-Methoden aufrufen, sofern ein Object-Handle vorliegt. Properties werden über `Object::get` und `Object::set` angebunden. Diese Entscheidung hält das Binding klein und stabil, auch wenn Godot-Klassen in neuen Versionen wachsen.

| Godot-Konzept | Lua-Repräsentation | Status |
|---|---|---|
| `nil`, `bool`, `int`, `float`, `String` | Primitive Lua-Werte | Implementiert |
| `Array` | 1-basierte Lua-Tabelle | Implementiert |
| `Dictionary` | Key-Value-Lua-Tabelle | Implementiert |
| `Vector2`, `Vector3`, `Color` | Konstruktorfunktionen und Tabellenrepräsentation | Implementiert |
| `Object` | Userdata mit Metatable | Implementiert |
| `Callable` | Lua-Funktion / Godot-Callable | Geplant |
| Signale | Connect/Disconnect aus Lua | Geplant |
| `RefCounted` | Starke Referenz-Policy | Geplant |
| `ScriptLanguageExtension` | `.lua` als Godot-Skript | Geplant |

## Build-Architektur

`nob.c` ist der einzige primäre Build-Runner. Er baut Lua, erzeugt bei Bedarf godot-cpp-Bindings aus der `extension_api.json`, kompiliert godot-cpp und linkt anschließend die GDExtension. Die generierten Dateien unter `thirdparty/godot-cpp/gen/` werden nicht versioniert, damit die Quelle der Wahrheit die Godot-API-Datei bleibt.

```text
nob.c
  ├─ build Lua 5.4 static library
  ├─ generate godot-cpp bindings when needed
  ├─ build godot-cpp static library
  ├─ build extension sources
  └─ link demo/addons/godot_lua/bin/libgodot_lua.*
```

## Produktionsausbau

Für den Einsatz in einem extrem großen Spielprojekt sollte dieses Fundament um projektinterne typed APIs ergänzt werden. Die empfohlene Strategie ist zweigleisig: Die dynamische Brücke bleibt für allgemeine Godot-Zugriffe verfügbar, während stabile Gameplay-Subsysteme zusätzliche handgeschriebene oder generierte Lua-Wrapper erhalten. So bleiben häufige Hotpath-Calls effizient, ohne die komplette Godot-API statisch nachbilden zu müssen.
