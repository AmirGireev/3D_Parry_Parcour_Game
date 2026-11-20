# Fortschritt

| Step | Status | Notizen |
|------|--------|---------|
| 1–8  | fertig | Vor Einführung dieser Datei abgeschlossen |
| 9    | fertig | BP_PlayerHero: bCounterReady, CounterWindow (1.5), DashTarget; Custom Event OnParrySuccess (Zeitlupe 0.2 für 0.03 s, Counter-Fenster per Retriggerable Delay); ParryZone BeginOverlap → Cast BP_projectileBase → DashTarget = Owner → OnParrySuccess → Projektil zerstören. Abweichung: OnParrySuccess hat keinen Attacker-Input (per MCP nicht anlegbar), DashTarget wird vor dem Aufruf gesetzt. |
