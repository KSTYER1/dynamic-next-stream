Dynamic Next Stream — OBS Dock Plugin v1.0.0
=============================================

Dock-Plugin fuer portable OBS-Installationen (Windows x64). Enthaelt:

  * Dock "Naechster Stream"  — Wochenplan-Editor mit Live-Vorschau

Verwandte Pakete:
  * dynamic-texts-1.3.0       — separat, enthaelt zwei Filter
                                (Dynamic Time/Date, Dynamic Countdown)


Inhalt
------

  dynamic-next-stream-1.0.0\
    INSTALL.bat                          <- Doppelklick = autom. Kopieren
    README.txt                           <- diese Datei
    obs-plugins\
      64bit\
        dynamic-next-stream.dll          <- Plugin-Binary (Qt6 erforderlich)
        dynamic-next-stream.pdb          <- Debug-Symbole, optional
    data\
      obs-plugins\
        dynamic-next-stream\
          locale\
            en-US.ini
            de-DE.ini


Installation
------------

Empfohlen: Doppelklick auf INSTALL.bat, OBS-Pfad eingeben, fertig.

Manuell: BEIDE Top-Level-Ordner (obs-plugins\, data\) in den OBS-Hauptordner
kopieren — ohne den data\-Teil zeigt das Dock rohe Schluessel statt Beschriftungen.


Verwendung
----------

Nach Neustart: Menue Ansicht → Andocken → "Naechster Stream" anklicken.

Im Dock:
  - Oben fixiert: Aktuelle Uhrzeit, Live-Countdown zum naechsten Stream,
                  Ziel-Textquelle waehlen, Live-Vorschau
  - Tab "Plan":   7 Wochentage (an/aus, Auto/Manuell-Zeit, Kategorie)
  - Tab "Format": Praefix, Trennzeichen, Suffix, Tagesformat (voll/kurz),
                  Anzahl Streams, Kategorie-Anzeige (Klammern), feste Stream-
                  Zeit ODER gerade/ungerade Kalenderwoche, Update-Intervall
  - Tab "Datei":  Optionaler TXT-Export mit Emoji-Modus, mehrzeilig, etc.

Auto-Save: jede Aenderung wird sofort in
<OBS>\config\obs-studio\plugin_config\dynamic-next-stream\next-stream.json
gespeichert.


Eigenschaften
-------------

  - QTimeEdit fuer alle Zeiten (HH:MM mit Spinner, Validierung)
  - ISO-8601-Kalenderwoche fuer gerade/ungerade Logik
  - Tooltips auf jedem Feld
  - "Heute"/"Morgen"-Ersetzung fuer Offset 0/1
  - Live-Vorschau und Status-Countdown updaten 1x/Sekunde


Anforderungen
-------------

  - OBS Studio 30.x / 31.x / 32.x (Windows x64)
  - Qt6 (von OBS mitgeliefert, nicht separat noetig)


Versionshistorie
----------------

1.0.0 (2026-04-26)
  * Erstmals als eigenstaendiges Plugin (vorher Teil von dynamic-texts 1.2.0)
  * 3-Tab-Layout (Plan / Format / Datei) mit fixiertem Status-Bereich
  * Live-Countdown zum naechsten Stream im Status-Bereich
  * QTimeEdit-Felder ueberall, Tooltips ueberall


Migration von dynamic-texts 1.2.0
---------------------------------

Falls bereits dynamic-texts 1.2.0 (kombiniert) installiert war:

  1. Auf dynamic-texts 1.3.0 updaten (Filter-only, separates Paket).
  2. Dieses Paket installieren.
  3. Optional: gespeicherten Plan migrieren:
       von <OBS>\config\obs-studio\plugin_config\dynamic-texts\next-stream.json
       nach <OBS>\config\obs-studio\plugin_config\dynamic-next-stream\next-stream.json


Lizenz: GPLv2 oder neuer.
