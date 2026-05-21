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
4. Ein nativer `GDExtensionScriptInstance`-Descriptor in `LuaScript`, der programmatisches `Node.set_script(LuaScript)` ohne GDScript-Wrapper ermöglicht.
5. `LuaScriptHost` als kopierbarer GDScript-Node, der `.lua`-Dateien per Export-Pfad lädt und als robuster Fallback die wichtigsten Node-Callbacks an Lua weiterleitet.
6. `godot_lua_editor` als Editor-Plugin, das `.lua`-Dateien importiert und ausgewählte Nodes über eine UndoRedo-fähige Menüaktion direkt mit `LuaScript` verbindet.

## Implementierungsstand 2026-05-21

Die erste Ausbaustufe ist umgesetzt und um den nativen Attach-Pfad sowie ein Editor-Plugin erweitert. `LuaScriptLanguage` wird in `register_types.cpp` als Godot-Skriptsprache registriert. `LuaScript` speichert und analysiert Lua-Quellcode, kann mit `load_from_file(path)` direkt aus dem Godot-Dateisystem befüllt werden und erzeugt über `_instance_create()` einen `GDExtensionScriptInstance`-Descriptor. Dadurch kann ein Node programmatisch per `node.set_script(lua_script)` Lua-Methoden, Properties und Lifecycle-Notifications nutzen. Das Demo-Addon `godot_lua_editor` ergänzt einen `.lua`-Importer und den Menüpunkt **Attach Lua Script to Selected Node...**, der ausgewählte Nodes im Editor über denselben nativen Attach-Pfad verbindet. `LuaScriptInstance` bleibt zusätzlich als Godot-exponierter `RefCounted`-Runtime-Wrapper verfügbar, der `self`/`owner` auf das Besitzerobjekt setzt, den Lua-Quellcode ausführt, den Skriptordner automatisch als Modul-Root einträgt, Initial-Properties übernimmt, Reload und Diagnostik bietet und Lifecycle-Hooks explizit dispatcht.

| Komponente | Status | Folgearbeit |
|---|---|---|
| `LuaScriptLanguage` | Implementiert und über `Engine::register_script_language()` angemeldet. | Editor-Completion, Debugger-Stack und Profiling können schrittweise ausgebaut werden. |
| `LuaScript` | Implementiert als `ScriptExtension`-Resource mit Source-Code-, Datei-, Methodendaten und nativem `GDExtensionScriptInstanceInfo3`-Pfad. | Tiefe Inspector-Script-Slot-UX fehlt noch für direkt auswählbare `.lua`-Dateien. |
| `LuaScriptInstance` | Implementiert als Runtime-Wrapper für Datei-Initialisierung, Properties, Reload, Diagnostik und Lifecycle-Dispatch. | Wird intern auch als Nutzlast des nativen ScriptInstance-Descriptors verwendet. |
| `LuaScriptHost` | Als GDScript-Brücke im Demo-Projekt vorhanden und sofort als Node-Komponente verwendbar. | Bleibt als Fallback sinnvoll, wenn ein Projekt bewusst GDScript-Host-Nodes bevorzugt. |
| `godot_lua_editor` | Editor-Plugin mit `.lua`-Importer und Menüaktion für ausgewählte Nodes. | Nächster Schritt ist eine tiefere normale Inspector-Script-Slot-Integration. |
| Typed API Generator | `tools/generate_api_wrappers.py` hinzugefügt und um `godot/runtime.lua` für globale Runtime-Helfer erweitert. | Kann später um Projekt-Exportvariablen, Signale und Resource-Schemas erweitert werden. |

## Einschränkung

Godot verlangt für eine vollständige Sprachintegration zahlreiche virtuelle Methoden, Editor-Hooks und einen nativen `GDExtensionScriptInstance`-Descriptor. Der Descriptor ist jetzt als Grundversion vorhanden und delegiert an den bestehenden `LuaScriptInstance`-Runtime-Wrapper. Import und Menü-Attach sind ebenfalls vorhanden. Tiefere Debugger-, Autocomplete-, normale Inspector-Script-Slot- und Property-Inspector-Integration bleibt als Ausbaupunkt dokumentiert.

## Referenzen

[1]: https://docs.godotengine.org/en/stable/classes/class_scriptlanguageextension.html "Godot ScriptLanguageExtension"
[2]: https://docs.godotengine.org/en/stable/classes/class_scriptextension.html "Godot ScriptExtension"
[3]: https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html "Godot GDExtension system"
