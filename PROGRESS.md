# Fortschritt

| Step | Status | Notizen |
|------|--------|---------|
| 1–8  | fertig | Vor Einführung dieser Datei abgeschlossen |
| 9    | fertig | BP_PlayerHero: bCounterReady, CounterWindow (1.5), DashTarget; Custom Event OnParrySuccess (Zeitlupe 0.2 für 0.03 s, Counter-Fenster per Retriggerable Delay); ParryZone BeginOverlap → Cast BP_projectileBase → DashTarget = Owner → OnParrySuccess → Projektil zerstören. Abweichung: OnParrySuccess hat keinen Attacker-Input (per MCP nicht anlegbar), DashTarget wird vor dem Aufruf gesetzt. |
| 10   | fertig | Counter-Dash: Variablen (DashSpeed 3500, DashDamage 34 …), BeginPlay speichert DefaultGravity; IA_Dash Started → CanDash → StartDash → Delay(GetDashDuration) → EndDash; Capsule-Overlap → HandleDashOverlap (Tag Enemy, einmal pro Dash, ApplyDamage). Abweichungen: Logik in Funktionen statt inline im EventGraph; Capsule-Overlap per „Assign OnComponentBeginOverlap“ in BeginPlay (Bound Event für geerbte Capsule per MCP nicht anlegbar). |
