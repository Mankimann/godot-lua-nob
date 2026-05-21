# ScriptLanguageExtension-Implementierungsnotizen

Diese Notizen fassen die für die LuaJIT-Erweiterung relevanten Godot-API-Punkte zusammen.

| Quelle | Relevanz |
|---|---|
| Godot `ScriptLanguageExtension` | Liefert die Sprachintegration: Name, Extension, Reserved Words, Code Validation, Completion, Debugging, Script-Erzeugung und globale Konstanten. |
| Godot `ScriptExtension` | Liefert die Resource-Repräsentation eines Skripts, inklusive Source-Code, Method-/Property-/Signal-Listen, Instanzierung und Reload-Verhalten. |
| Godot GDExtension-System | Native Shared Libraries können ohne Engine-Fork geladen werden; das ist weiterhin die richtige Basis für dieses Binding. |

## Implementierungsstrategie

Die erste produktionsnahe Stufe liefert eine robuste Foundation, ohne zu behaupten, schon denselben Editor-Komfort wie GDScript zu erreichen. Dafür wurden drei Ebenen eingeführt:

1. `LuaScriptLanguage` als registrierbare `ScriptLanguageExtension`-Klasse für Spracheigenschaften, `.lua`-Erkennung, Templates und Syntaxvalidierung.
2. `LuaScript` als `ScriptExtension`-Resource für Source-Code, Pfadhinweis, Methodenscans und Resource-Metadaten.
3. `LuaScriptInstance` als Godot-Object-gebundene Laufzeitbrücke, die einen Lua-Chunk lädt, `self` und `owner` auf das Owner-Object setzt, Initial-Properties übernimmt und Lifecycle-Methoden dispatcht.
4. `LuaScriptHost` als kopierbarer GDScript-Node, der `.lua`-Dateien per Export-Pfad lädt und die wichtigsten Node-Callbacks an Lua weiterleitet.

## Implementierungsstand 2026-05-21

Die erste Ausbaustufe ist umgesetzt. `LuaScriptLanguage` wird in `register_types.cpp` als Godot-Skriptsprache registriert. `LuaScript` speichert und analysiert Lua-Quellcode und kann mit `load_from_file(path)` direkt aus dem Godot-Dateisystem befüllt werden. `LuaScriptInstance` ist ein Godot-exponierter `RefCounted`-Runtime-Wrapper, der `self`/`owner` auf das Besitzerobjekt setzt, den Lua-Quellcode ausführt, den Skriptordner automatisch als Modul-Root einträgt, Initial-Properties übernimmt, Reload und Diagnostik bietet und Lifecycle-Hooks explizit dispatcht.

| Komponente | Status | Folgearbeit |
|---|---|---|
| `LuaScriptLanguage` | Implementiert und über `Engine::register_script_language()` angemeldet. | Editor-Completion, Debugger-Stack und Profiling können schrittweise ausgebaut werden. |
| `LuaScript` | Implementiert als `ScriptExtension`-Resource mit Source-Code-, Datei- und Methodendaten. | `_instance_create()` muss für vollautomatische Node-Anbindung noch den nativen `GDExtensionScriptInstanceInfo3`-Pfad verwenden. |
| `LuaScriptInstance` | Implementiert als sicherer Übergangspfad für Datei-Initialisierung, Properties, Reload, Diagnostik und expliziten Lifecycle-Dispatch. | Nach Fertigstellung des C-API-Descriptors kann der Wrapper intern als private Nutzlast verwendet werden. |
| `LuaScriptHost` | Als GDScript-Brücke im Demo-Projekt vorhanden und sofort als Node-Komponente verwendbar. | Kann später als Addon-Vorlage mit Inspector-UX und Fehlerpanel verpackt werden. |
| Typed API Generator | `tools/generate_api_wrappers.py` hinzugefügt und um `godot/runtime.lua` für globale Runtime-Helfer erweitert. | Kann später um Projekt-Exportvariablen, Signale und Resource-Schemas erweitert werden. |

## Einschränkung

Godot verlangt für eine vollständige Sprachintegration zahlreiche virtuelle Methoden und einen nativen `GDExtensionScriptInstance`-Descriptor. Diese Iteration implementiert bewusst eine konservative Foundation mit Methodenscan, Lifecycle-Dispatch und Generator-Unterstützung. Tiefere Debugger-, Autocomplete-, Inspector- und native Node-Attach-Integration bleibt als Ausbaupunkt dokumentiert.

## Referenzen

[1]: https://docs.godotengine.org/en/stable/classes/class_scriptlanguageextension.html "Godot ScriptLanguageExtension"
[2]: https://docs.godotengine.org/en/stable/classes/class_scriptextension.html "Godot ScriptExtension"
[3]: https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html "Godot GDExtension system"
