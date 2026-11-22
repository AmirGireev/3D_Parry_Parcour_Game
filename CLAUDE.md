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
- Commit-Nachrichten sinnvoll schreiben: eine kurze, natürliche deutsche
  Betreffzeile mit echten Umlauten (ä, ö, ü – nie ae/oe/ue), die sagt, was
  sich im Spiel ändert (z. B. "Gegner-Basisklasse mit Hitbox und Stagger").
  Nur wenn nötig ein paar kurze Stichpunkte darunter, ohne interne Namen
  wie Funktions- oder Node-IDs. Keine Nummern, Kürzel oder "fix".
  Nachricht per Datei mit UTF-8 übergeben (git commit -F), damit Umlaute
  korrekt ankommen.
- Antworte kurz: was erledigt ist, was getestet wurde, was ich im Spiel
  selbst prüfen soll.
- Bei Werten für das Spielgefühl nimm die Startwerte aus dem Guide und
  frag nicht nach.

## Eigene Mechaniken (nicht im Guide – nie durch einen Step überschreiben)
Wenn ein Guide-Step etwas davon ersetzen, entfernen oder anders lösen will,
erst nachfragen. Details und Werte stehen in PROGRESS.md.
- Zwei getrennte Dashes: Shift (IA_Dash) = normaler Dash in Eingaberichtung,
  0,75 s Cooldown, kein Schaden. Linksklick (IA_CounterDash) = Counter-Dash,
  nur nach Parry oder in der Ketten-Phase, ohne Cooldown.
- Markieren: Kontakt (MarkZone am Hero) mit Gegnern mit Tag Enemy merkt sie
  vor; erst wenn man sich wieder entfernt (Ende des Overlaps), werden sie
  markiert, gelber Rand (M_MarkOutline als Overlay). Das Ziel eines Counter-Dashs ist danach
  1,5 s nicht markierbar (nur dieses Ziel), während eines Dashs nie.
  Markieren kostet eine Ausweich-Ladung; ohne Ladung wird nicht markiert.
- Ausweich-Ladungen: Wer in der Nahkampf-Hitbox stand und sie in den letzten
  0,4 s vor dem Treffer verlässt (Dash oder Laufen), bekommt einen Ausweicher
  (BP_EnemyBase prüft beim Einschalten der Hitbox, DodgeWindow 0,4).
  2 Ausweicher = 1 Ladung, max. 2 Ladungen. Anzeige: blaue Vierecke unter
  der Lebensanzeige, halb transparent = 1 Ausweicher Fortschritt.
- Ein Counter-Dash durch einen markierten Gegner heilt 1 Leben (bis max.).
- Counter-Dash-Schaden: markiert 2, sonst 1 (ersetzt den Guide-Wert 34 und
  den Bonus-Schaden aus Step 17). Kleine Gegner 3 Leben (auch Fernkämpfer,
  statt Guide-Wert 40), Turm 10. Spieler 5 Leben, jeder gegnerische Treffer 1.
- Lebensanzeigen in Vierecken (WBP_SegmentBar aus WBP_Segment): Spieler grün
  oben links (1 Viereck pro Leben), Gegner rot über dem Kopf.
- Gegner-Angriff im Behavior Tree: ein gemeinsamer Task (BTT_MeleeAttack)
  ruft je nach Gegnertyp StartAttack bzw. StartShoot auf. Fernkämpfer
  schaut per Controller-Fokus immer zum Spieler.
- Kette: Sind nach einem Counter-Dash noch Gegner markiert, löst Linksklick
  innerhalb von 1 s den nächsten Counter-Dash zum nächsten markierten Gegner aus.
- Parry: 0,75 s Zeitlupe (Faktor 0,2), Counter-Dash beendet sie sofort.
- Counter-Dash fliegt in 3D auf die Mitte der Kollisions-Bounds des Ziels
  (auch nach oben/unten) und 400 darüber hinaus.
- Bewegung: Sprung und Dash in Eingaberichtung, Coyote-Time 0,15 s,
  Laufen 900, Jump Z 1000, Gravity 2,8. Nach dem Dash bleibt Schwung
  (80 % der Dash-Geschwindigkeit, max. +700 über Laufen) und baut sich weich
  ab; Springen im/direkt nach dem Dash bricht ihn ab und behält den Schwung
  in der Luft (Weitsprung). Am Sprunghöhepunkt (|Vz| < 250) Schwerkraft ×0,55,
  beim Fallen ×1,15. Sprung-Puffer 0,12 s vor der Landung.
- Gegner-KI: Erkennungsradius statt Dauer-Verfolgung. BP_EnemyBase prüft alle
  0,25 s (UpdateTarget): Spieler im DetectRadius → TargetActor + Fokus,
  weiter als 1,5 × Radius → Ziel gelöscht. Nahkämpfer 1500, Fernkämpfer 2200,
  Turm schießt nur bis 2500. Fernkämpfer weicht unter RetreatDistance (900)
  zurück und macht nach jedem Schuss einen Seitenschritt. Die KI-Controller
  starten nur den Behavior Tree.
- Angriffs-Warnung: Jeder Gegner (BP_EnemyBase, BP_Turret und alle neuen)
  zeigt 0,5 s vor dem Angriff ein rotes „!“ über dem Kopf (WBP_AttackWarning
  als Widget-Komponente AttackWarning). PlayTelegraph = dieses Symbol, kein
  Aufblähen. Neue Gegner müssen das ebenfalls haben.
- Parry-Kugel (ParryVisual) ist im Spiel unsichtbar (Hidden in Game).
- Nahkämpfer nutzt ABP_Manny_Combat und greift zufällig mit Heavy Attack
  (AM_ChargedAttack, 1 Treffer) oder Combo (AM_ComboAttack, 3 Treffer) aus
  Variant_Combat an. Fernkämpfer schießt zwei Projektile hintereinander auf
  derselben Bahn (einmal zielen, gleicher Startpunkt), Projektile folgen dem
  Spieler leicht (Homing 1200) und leben nur 3 s.
- Combo-Parry: Jeder Angriff hat eine Trefferzahl (Combo 3, Heavy 1,
  Fernkämpfer-Salve 2, Turm 1). Jeder geparierte Treffer blockt nur; erst der
  letzte geparierte Treffer löst Zeitlupe, Counter-Fenster und Stagger aus.
  Trifft ein Teil der Combo den Spieler, gibt es für diesen Angriff keinen
  Counter. Teil-Parry = grüne Funken (NS_ParrySpark_Green), letzter Parry =
  rote Funken (NS_ParrySpark_Red).
- Nahkämpfer blitzt 0,1 s vor jedem Treffer rot auf (FlashRed in BP_EnemyBase,
  Overlay M_HitFlash 0,15 s, vorheriges Overlay wie die Markierung bleibt
  erhalten). In StartAttack steht vor jedem HitboxOn: Delay (alt − 0,1) →
  FlashRed → Delay 0,1.
- Spielfigur: Paragon Gideon (Scale 0,85, kleiner als die Gegner) mit seinem
  eigenen Animation Blueprint, Portal-Ring und Start-Animation ausgeblendet.
- Dash bewegt die Figur im Flying-Modus (keine Sprung-/Landeanimation), ohne
  Dash-Animation und ohne FOV-Änderung.
- Lock-On (Universal Lock-On, mittlere Maustaste) als Soft Lock: Ziel wird
  nur markiert (Anzeige auf spine_04) und ist Ziel des Counter-Dashs; Kamera
  und Figur bleiben frei (allowTracking aus, Kamera-Steuerung Freecam, Figur
  dreht in Laufrichtung). Beim Anvisieren dreht die Kamera einmal zum Ziel
  (nur Yaw), danach ist sie wieder frei. Dreht die Kamera mehr als 40° vom
  Ziel weg (LockViewAngle), löst sich das Lock-On und man kann neu anvisieren.
  Das anvisierte Ziel zeigt seine Lebensanzeige über dem Kopf (Widget-
  Komponente HealthBar mit WBP_SegmentBar; der Hero richtet sie zur
  Laufzeit ein). Neue Gegner brauchen nur eine Widget-Komponente HealthBar
  und AC_Health.
- Controller: X/A = Springen, L1 = Parry, R1 = Dash, R2 = Counter-Dash.
- Kamera: Arm 550, Socket-Offset Z 150, Kamera 10° nach unten geneigt, kein
  Camera Lag.

## Konventionen
- Ordner und Namenspräfixe exakt wie in Step 1 (_Game/..., BP_, ABP_,
  AM_, AC_, IA_, IMC_, WBP_ ...).
- Assets nur über den Editor/MCP verschieben, umbenennen oder löschen.

## Tabu
- Keine Engine-Version ändern, keine Plugins oder Marketplace-Assets
  ohne Rückfrage installieren.
- Nichts löschen, das nicht zum aktuellen Step gehört.
- Vor großen Änderungen speichern.
