# Projekt: Parry-Dash-Game (Unreal Engine 5.8, Blueprint)

## Ziel
3D-Parry-Parkour-Spiel. Kern: Parry → kurze Zeitlupe → Counter-Dash
durch den Gegner. Ziel ist ein spielbarer Ausschnitt mit Parkour-Strecke,
kleinen Gegnern und Riesen-Boss. Spielgefühl ist wichtiger als Optik.

## Stand
Step 1 bis 8 des Guides sind fertig. Wir machen jetzt weiter ab Step 9.
Nach jedem erledigten Step trägst du ihn in PROGRESS.md ein.

## Arbeitsweise
- Der Guide liegt im Ordner docs/ (docs/Parry-Dash-Game_Entwicklungs-Guide.pdf).
  Wir arbeiten strikt Step für Step. Ich nenne den Step, mach nur diesen,
  nicht den nächsten.
- Geh nicht blind nach der PDF. Sie gibt nur die Richtung vor. Maßgeblich
  ist, was ich dir sage.
- Setze den Step selbst über Unreal MCP im Editor um, statt ihn nur zu
  beschreiben. Prüfe vorher, was in Steps 1 bis 8 schon existiert, und
  baue darauf auf, statt Dinge doppelt anzulegen.
- Der Guide ist der Plan, nicht heilig: Wenn in 5.8 ein Node, Menü oder
  Wert anders heißt oder der Guide einen Fehler hat, löse es sinnvoll und
  nenn mir die Abweichung in einer Zeile.
- Ist ein Schritt nur manuell möglich (externe Tools wie Meshy/Mixamo,
  Asset-Download, Spielgefühl testen), sag mir genau, was ich tun soll.
- Nach jedem Step: Blueprints kompilieren, speichern, Output Log auf
  Fehler prüfen, den Test des Steps ausführen, soweit möglich. Dann
  PROGRESS.md aktualisieren und committen.
- Commit-Nachrichten sinnvoll schreiben: eine aussagekräftige Betreffzeile,
  die sagt, was sich im Spiel ändert (z. B. "Counter-Dash: hinter Gegner
  und schneller"), plus kurze Stichpunkte zum Wie. Keine nichtssagenden
  Nachrichten wie Nummern, Kürzel oder "fix".
- Antworte kurz: was erledigt ist, was getestet wurde, was ich im Spiel
  selbst prüfen soll.
- Bei Werten für das Spielgefühl nimm die Startwerte aus dem Guide und
  frag nicht nach.

## Konventionen
- Ordner und Namenspräfixe exakt wie in Step 1 (_Game/..., BP_, ABP_,
  AM_, AC_, IA_, IMC_, WBP_ ...).
- Assets nur über den Editor/MCP verschieben, umbenennen oder löschen.

## Tabu
- Keine Engine-Version ändern, keine Plugins oder Marketplace-Assets
  ohne Rückfrage installieren.
- Nichts löschen, das nicht zum aktuellen Step gehört.
- Vor großen Änderungen speichern.
