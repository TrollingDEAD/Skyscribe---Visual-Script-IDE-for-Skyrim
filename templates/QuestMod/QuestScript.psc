ScriptName MyQuestScript extends Quest

; ── Properties ────────────────────────────────────────────────────────────────
; Add your quest properties here, e.g.:
; Actor Property PlayerRef Auto

; ── Events ────────────────────────────────────────────────────────────────────

Event OnInit()
    ; Called when the quest first starts.
    Debug.Notification("Quest started!")
EndEvent

Function SetStage(Int akStage)
    ; Handle individual quest stages.
    If (akStage == 10)
        ; Stage 10 logic here.
    ElseIf (akStage == 20)
        ; Stage 20 logic here.
    EndIf
EndFunction
