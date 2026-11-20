# Fortschritt

| Step | Status | Notizen |
|------|--------|---------|
| 1–8  | fertig | Vor Einführung dieser Datei abgeschlossen |
| 9    | fertig | BP_PlayerHero: bCounterReady, CounterWindow (1.5), DashTarget; Custom Event OnParrySuccess (Zeitlupe 0.2 für 0.03 s, Counter-Fenster per Retriggerable Delay); ParryZone BeginOverlap → Cast BP_projectileBase → DashTarget = Owner → OnParrySuccess → Projektil zerstören. Abweichung: OnParrySuccess hat keinen Attacker-Input (per MCP nicht anlegbar), DashTarget wird vor dem Aufruf gesetzt. |
| 10   | fertig | Counter-Dash: Variablen (DashSpeed 3500, DashDamage 34 …), BeginPlay speichert DefaultGravity; IA_Dash Started → CanDash → StartDash → Delay(GetDashDuration) → EndDash; Capsule-Overlap → HandleDashOverlap (Tag Enemy, einmal pro Dash, ApplyDamage). Abweichungen: Logik in Funktionen statt inline im EventGraph; Capsule-Overlap per „Assign OnComponentBeginOverlap“ in BeginPlay (Bound Event für geerbte Capsule per MCP nicht anlegbar). |
| 10a  | fertig | Nachbesserung: normaler Dash geht jetzt immer (800 nach vorn, DashSpeed); Counter-Dash nur im Counter-Fenster, horizontal, Ziel = hinter dem Gegner (+400), schneller (CounterDashSpeed 6000). Coyote-Time 0.15 s: OnWalkingOffLedge erlaubt kurz einen vollen Boden-Sprung (JumpMaxCount 3 → zurück auf 2 nach Fenster/Landung). |
| 10b  | fertig | Dashes getrennt: IA_Dash (Shift) = nur normaler Dash (StartDash, 800 vorwärts, DashSpeed). Counter-Dash = Parry-Taste im Counter-Fenster (IA_Parry Started → CanCounterDash → StartCounterDash, sonst normale Parry-Logik). HandleDashOverlap prüft IsValid (Fehler bei zerstörten Projektilen behoben). |
