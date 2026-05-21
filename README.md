# Godot Lua Nob

**Godot Lua Nob** ist ein produktionsorientiertes Starter-Repository für ein Lua-5.4-Binding in Godot 4. Das Projekt verwendet **GDExtension**, bindet Lua statisch ein und nutzt **nob.h** als primäres, selbsthostendes Build-System. Die Architektur ist bewusst auf große Spielprojekte ausgelegt: keine Engine-Fork, kein vollständiger statischer Wrapper für jede Godot-Klasse, sondern eine wartbare dynamische Brücke über `Variant`, `Object`, Singletons und Godot-Methoden.

> Godot 4 GDExtension ist die moderne native Erweiterungsschnittstelle für Godot, bei der externe Shared Libraries zur Laufzeit geladen werden. Für langfristige Projekte ist dieser Ansatz in der Regel wartbarer als ein eigener Engine-Fork, weil das Spiel mit offiziellen Godot-Releases aktualisiert werden kann.[1] Die C++-Bindings werden über `godot-cpp` bereitgestellt und aus der passenden `extension_api.json` generiert.[2]

## Architekturentscheidung

Für ein sehr großes Spielprojekt ist die beste Basis **Godot 4 + GDExtension + Lua 5.4 + nob.h**. GDExtension reduziert die Wartungskosten, Lua 5.4 bleibt portabel und vorhersagbar, und nob.h hält das Build-System transparent, versionierbar und debuggbar. Eine dynamische Godot-Brücke ist in diesem Kontext robuster als ein statisch generierter Lua-Wrapper für jede Godot-Klasse, weil Godot-APIs über Minor-Versionen wachsen und sich Details ändern können.

| Bereich | Entscheidung | Begründung |
|---|---:|---|
| Godot-Integration | **GDExtension** | Native Performance ohne Engine-Fork und mit klarer Godot-4-Kompatibilität.[1] |
| C++-Binding | **godot-cpp** | Offizielle C++-Schicht über der GDExtension-C-API.[2] |
| Lua-Version | **Lua 5.4.x** | Portabler und einfacher zu sandboxen als JIT-basierte Laufzeiten. |
| Build-System | **nob.h** | Selbsthostender C-Build-Runner, der im Repository liegt und keine Generator-Magie versteckt.[3] |
| Godot-API-Modell | **Dynamische Variant/Object-Brücke** | Besser wartbar als tausende handgeschriebene Lua-Wrapper. |
| Projektstatus | **Foundation / MVP** | Enthält Build, Runtime, Variant-Konvertierung, Demo und CI; ScriptLanguage-Integration ist als nächste Ausbaustufe vorgesehen. |

## Repository-Struktur

Das Repository ist so aufgebaut, dass Gameplay-Teams die Runtime direkt ausprobieren und Engine-/Tools-Teams sie schrittweise erweitern können. `nob.c` ist der zentrale Build-Runner; `tools/generate_godot_cpp.py` erzeugt die fehlenden godot-cpp-Klassenbindungen direkt aus `extension_api.json`, ohne SCons zu benötigen.

| Pfad | Zweck |
|---|---|
| `nob.c` | Primäres Build-System mit Lua-Build, godot-cpp-Codegenerierung, statischen Libraries und finaler GDExtension. |
| `include/godot_lua/` | Öffentliche C++-Header für Runtime, Fehlerobjekte und Variant-Brücke. |
| `src/runtime/` | `LuaState` und `LuaError` als Godot-registrierbare Klassen. |
| `src/bindings/` | Konvertierung zwischen Lua-Werten und Godot-`Variant`, Objektzugriff, Singletons und Utility-Funktionen. |
| `thirdparty/lua/` | Eingebetteter Lua-5.4-Quellcode. |
| `thirdparty/godot-cpp/` | Offizielle Godot-C++-Bindings. Generierte Dateien liegen unter `thirdparty/godot-cpp/gen/` und werden nicht versioniert. |
| `demo/` | Minimales Godot-Projekt mit `.gdextension`, Szene, GDScript und Lua-Beispiel. |
| `.github/workflows/build.yml` | Linux-CI, die `nob` bootstrapped und die Extension baut. |

## Build

Der Build ist absichtlich schlicht. Zunächst wird der Build-Runner aus `nob.c` kompiliert, anschließend übernimmt `./nob` die vollständige Pipeline. Auf einem typischen Linux-Entwicklungsrechner werden ein C-Compiler, ein C++17-Compiler, `ar` und Python 3 benötigt.

```bash
cc -o nob nob.c
./nob build debug platform=linux arch=x86_64
```

Für Release-Builds wird der Modus entsprechend geändert.

```bash
./nob build release platform=linux arch=x86_64
```

Die erzeugte Shared Library landet unter `demo/addons/godot_lua/bin/`. Die `.gdextension`-Datei im Demo-Projekt zeigt bereits auf die üblichen Debug- und Release-Pfade. Wenn ein anderes Toolchain-Setup verwendet werden soll, können `CC`, `CXX` und `AR` überschrieben werden.

```bash
CC=clang CXX=clang++ AR=llvm-ar ./nob build release
```

## Lua API im aktuellen Fundament

Die erste Version stellt eine bewusst kompakte, aber erweiterbare Runtime bereit. `LuaState` kann Code-Strings und Dateien ausführen, globale Werte setzen oder lesen und optional Sandbox-Einschränkungen aktivieren. Im Lua-Umfeld steht eine globale Tabelle `godot` bereit.

| Lua-Funktion | Beschreibung |
|---|---|
| `godot.print(...)` | Schreibt über Godots `UtilityFunctions::print` in die Godot-Konsole. |
| `godot.get_singleton(name)` | Gibt ein Godot-Singleton wie `Engine`, `Input` oder `ProjectSettings` als Lua-Objekt zurück. |
| `godot.variant_type(value)` | Liefert den Godot-Variant-Typnamen eines Lua-Werts. |
| `Vector2(x, y)` | Erzeugt einen Godot-`Vector2` und gibt ihn nach Lua zurück. |
| `Vector3(x, y, z)` | Erzeugt einen Godot-`Vector3`. |
| `Color(r, g, b, a)` | Erzeugt eine Godot-`Color`. |

Godot-Objekte werden in Lua als Userdata gehalten. Methodenaufrufe erfolgen dynamisch über Godots `Object::callv`; Properties werden über `Object::get` und `Object::set` angebunden. Dadurch kann dieselbe Brücke mit beliebigen Godot-Klassen arbeiten, ohne für jede Klasse eine separate Lua-Datei oder C++-Binding-Datei zu generieren.

## Demo

Das Demo-Projekt liegt in `demo/`. Nach einem erfolgreichen Build kann der Ordner in Godot 4 geöffnet werden. Die Szene `demo/scenes/main.tscn` startet `demo/scripts/main.gd`, erzeugt eine `LuaState`-Instanz und führt `demo/scripts/hello.lua` aus. Das Beispiel demonstriert Godot-Ausgaben, Vektoren, Sandbox-Verhalten und Singleton-Zugriffe.

## Sicherheit und Sandbox

Der Sandbox-Modus entfernt im aktuellen Fundament besonders riskante Lua-Funktionen wie direkte Prozessausführung und Dateisystemzugriffe über `io` und `package`. Für ein großes Spielprojekt sollte diese Schicht später rollenbasiert erweitert werden, beispielsweise getrennt für interne Gameplay-Skripte, Modding-Skripte, Editor-Tools und Live-Ops-Konfigurationen.

> Die Sandbox ist ein **Sicherheitsfundament**, kein endgültiges Sicherheitsmodell. Für Modding in Produktion müssen Pfad-Policies, Ressourcenlimits, deterministische Timeouts, Speicherbudgets und gegebenenfalls getrennte Lua-States pro Vertrauenszone ergänzt werden.

## Roadmap für ein wirklich vollständiges Binding

Dieses Repository ist als kompilierbares Fundament angelegt. Die nächsten Ausbaustufen sollten iterativ erfolgen, damit das Binding in einem großen Projekt kontrolliert wächst und nicht durch eine riesige, schwer wartbare API-Schicht blockiert wird.

| Priorität | Erweiterung | Nutzen |
|---:|---|---|
| 1 | `Callable`- und Signal-Brücke | Lua-Funktionen als Godot-Callbacks und Signal-Handler. |
| 2 | `RefCounted`/`Resource`-Lifetime-Policies | Sichere Referenzhaltung für Ressourcen und Gameplay-Daten. |
| 3 | `ScriptLanguageExtension` | Lua-Skripte als echte Godot-Skripte im Editor. |
| 4 | `require` für `res://` und `user://` | Projektweite Lua-Modularisierung. |
| 5 | Hot-Reload und Editor-Diagnostics | Schneller Iterationszyklus für Gameplay-Teams. |
| 6 | Test-Runner für Headless-Godot | CI-validierte Integrationstests. |
| 7 | Typed wrapper generator für projektinterne APIs | Optionale, performantere Lua-Wrapper für stabile eigene Gameplay-APIs. |

## Lizenzhinweis

Dieses Repository enthält Third-Party-Quellen. Vor einem öffentlichen Release müssen die jeweiligen Lizenzdateien von Lua, godot-cpp und nob.h geprüft und vollständig beibehalten werden. Für dein privates Projekt ist der aktuelle Aufbau geeignet; für Distributionen sollte zusätzlich eine `THIRD_PARTY_NOTICES.md` gepflegt werden.

## References

[1]: https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html "Godot Documentation: GDExtension"
[2]: https://github.com/godotengine/godot-cpp "godotengine/godot-cpp"
[3]: https://github.com/tsoding/nob.h "tsoding/nob.h"
[4]: https://github.com/gilzoide/lua-gdextension "gilzoide/lua-gdextension"
