ScriptName MyAliasScript extends ReferenceAlias

; ── Properties ────────────────────────────────────────────────────────────────
; Actor Property PlayerRef Auto

; ── Events ────────────────────────────────────────────────────────────────────

Event OnInit()
    Actor akTarget = GetActorReference()
    If akTarget != None
        Debug.Notification("Alias attached to: " + akTarget.GetDisplayName())
    EndIf
EndEvent

Event OnLoad()
    ; Called when the aliased actor loads into a cell.
EndEvent

Event OnDeath(Actor akKiller)
    ; Called when the aliased actor dies.
    Debug.Notification(GetActorReference().GetDisplayName() + " has died.")
EndEvent
