#include "graph/BuiltinNodes.h"
#include "graph/NodeRegistry.h"
#include "graph/NodeDefinition.h"

// Helper macros to reduce boilerplate.
// Usage: BEGIN_NODE("builtin.OnInit", "OnInit", Event, "tooltip") ... END_NODE
// Pins are added with EXEC_OUT, EXEC_IN, DATA_OUT, DATA_IN.

namespace {

using namespace graph;

static PinDefinition ExecIn() {
    PinDefinition p; p.name = "In"; p.kind = PinKind::Input;
    p.flow = PinFlow::Execution; p.type = PinType::Exec; return p;
}
static PinDefinition ExecOut(const char* name = "Out") {
    PinDefinition p; p.name = name; p.kind = PinKind::Output;
    p.flow = PinFlow::Execution; p.type = PinType::Exec; return p;
}
static PinDefinition DataOut(const char* name, PinType t, const char* tooltip = "") {
    PinDefinition p; p.name = name; p.kind = PinKind::Output;
    p.flow = PinFlow::Data; p.type = t; p.tooltip = tooltip; return p;
}
static PinDefinition DataIn(const char* name, PinType t,
                            const char* default_val = "", const char* tooltip = "") {
    PinDefinition p; p.name = name; p.kind = PinKind::Input;
    p.flow = PinFlow::Data; p.type = t;
    p.default_value = default_val; p.tooltip = tooltip; return p;
}

static void Reg(NodeDefinition def) {
    NodeRegistry::Get().Register(std::move(def));
}

static NodeDefinition N(const char* type_id, const char* display_name,
                        NodeCategory cat, const char* tooltip = "",
                        const char* codegen = "") {
    NodeDefinition d;
    d.type_id       = type_id;
    d.display_name  = display_name;
    d.category      = cat;
    d.tooltip       = tooltip;
    d.codegen_template = codegen;
    return d;
}

} // anonymous namespace

namespace graph {

void BuiltinNodes::RegisterAll() {
    // ── Event Nodes ───────────────────────────────────────────────────────────
    {
        auto d = N("builtin.OnInit", "OnInit", NodeCategory::Event,
                   "Fires when the script is first loaded.");
        d.pins = { ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnActivate", "OnActivate", NodeCategory::Event,
                   "Fires when the object is activated by an actor.");
        d.pins = { ExecOut(), DataOut("akActionRef", PinType::Actor, "The actor that activated this object") };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnLoad", "OnLoad", NodeCategory::Event,
                   "Fires when the cell containing this object is loaded.");
        d.pins = { ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnUnload", "OnUnload", NodeCategory::Event,
                   "Fires when the cell is unloaded.");
        d.pins = { ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnDeath", "OnDeath", NodeCategory::Event,
                   "Fires when this actor dies.");
        d.pins = { ExecOut(), DataOut("akKiller", PinType::Actor, "The actor that delivered the killing blow") };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnHit", "OnHit", NodeCategory::Event,
                   "Fires when this reference is hit.");
        d.pins = { ExecOut(), DataOut("akAggressor", PinType::ObjectRef), DataOut("akWeapon", PinType::Form) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnEquipped", "OnEquipped", NodeCategory::Event,
                   "Fires when this item is equipped by an actor.");
        d.pins = { ExecOut(), DataOut("akActor", PinType::Actor) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnUnequipped", "OnUnequipped", NodeCategory::Event,
                   "Fires when this item is unequipped by an actor.");
        d.pins = { ExecOut(), DataOut("akActor", PinType::Actor) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnCombatStateChanged", "OnCombatStateChanged", NodeCategory::Event,
                   "Fires when an actor's combat state changes.");
        d.pins = { ExecOut(), DataOut("akTarget", PinType::Actor), DataOut("aiCombatState", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnTimer", "OnTimer", NodeCategory::Event,
                   "Fires when a timer started with StartTimer() expires.");
        d.pins = { ExecOut(), DataOut("aiTimerID", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnUpdateGameTime", "OnUpdateGameTime", NodeCategory::Event,
                   "Fires each in-game hour when registered.");
        d.pins = { ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.OnStageSet", "OnStageSet", NodeCategory::Event,
                   "Fires when a quest stage is set.");
        d.pins = { ExecOut(), DataOut("auiStageID", PinType::Int), DataOut("auiItemID", PinType::Int) };
        Reg(std::move(d));
    }

    // ── Control Flow Nodes ────────────────────────────────────────────────────
    {
        auto d = N("builtin.IfElse", "If / Else", NodeCategory::ControlFlow,
                   "Branches execution based on a boolean condition.",
                   "If {Condition}");
        d.pins = { ExecIn(), DataIn("Condition", PinType::Bool),
                   ExecOut("True"), ExecOut("False") };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.WhileLoop", "While Loop", NodeCategory::ControlFlow,
                   "Repeats the loop body while a condition is true.",
                   "While {Condition}");
        d.pins = { ExecIn(), DataIn("Condition", PinType::Bool),
                   ExecOut("Loop Body"), ExecOut("Completed") };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Return", "Return", NodeCategory::ControlFlow,
                   "Terminates execution and optionally returns a value.");
        d.pins = { ExecIn() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Sequence", "Sequence", NodeCategory::ControlFlow,
                   "Executes multiple execution paths in order.");
        d.pins = { ExecIn(), ExecOut("Then 0"), ExecOut("Then 1"), ExecOut("Then 2") };
        Reg(std::move(d));
    }

    // ── Variable Nodes ────────────────────────────────────────────────────────
    {
        auto d = N("builtin.SetVariable", "Set Variable", NodeCategory::Variable,
                   "Assigns a value to a variable.", "{Variable} = {Value}");
        d.pins = { ExecIn(), DataIn("Variable", PinType::Unknown),
                   DataIn("Value", PinType::Unknown), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GetVariable", "Get Variable", NodeCategory::Variable,
                   "Reads the current value of a variable.");
        d.pins = { DataOut("Value", PinType::Unknown) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.DeclareLocal", "Declare Local", NodeCategory::Variable,
                   "Declares a typed local variable, hoisted to the top of the event/function block.");
        d.pins = {
            ExecIn(), ExecOut(),
            DataIn("Name", PinType::String, "kVar",     "Variable name"),
            DataIn("Type", PinType::String, "Int",      "Papyrus type (Int, Float, Bool, String, Actor, …)"),
            DataIn("InitialValue", PinType::String, "", "Optional initial value"),
            DataOut("Ref", PinType::Unknown,             "Reference to the declared variable"),
        };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.LiteralInt", "Literal Int", NodeCategory::Variable,
                   "An integer constant.", "{Value}");
        d.pins = { DataOut("Value", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.LiteralFloat", "Literal Float", NodeCategory::Variable,
                   "A float constant.", "{Value}");
        d.pins = { DataOut("Value", PinType::Float) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.LiteralString", "Literal String", NodeCategory::Variable,
                   "A string constant.", "\"{Value}\"");
        d.pins = { DataOut("Value", PinType::String) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.LiteralBool", "Literal Bool", NodeCategory::Variable,
                   "A boolean constant (true or false).", "{Value}");
        d.pins = { DataOut("Value", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Cast", "Cast", NodeCategory::Variable,
                   "Explicitly casts a value to a different type.", "{Value} as {Type}");
        d.pins = { DataIn("Value", PinType::ObjectRef), DataOut("Result", PinType::ObjectRef) };
        Reg(std::move(d));
    }

    // ── Math & Logic Nodes ────────────────────────────────────────────────────
    {
        auto d = N("builtin.AddInt", "Add (Int)", NodeCategory::Math,
                   "Adds two integers.", "{A} + {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.SubtractInt", "Subtract (Int)", NodeCategory::Math,
                   "Subtracts B from A.", "{A} - {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.MultiplyInt", "Multiply (Int)", NodeCategory::Math,
                   "Multiplies two integers.", "{A} * {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.DivideInt", "Divide (Int)", NodeCategory::Math,
                   "Divides A by B.", "{A} / {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "1"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Modulo", "Modulo", NodeCategory::Math,
                   "A modulo B.", "{A} % {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "1"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.AddFloat", "Add (Float)", NodeCategory::Math,
                   "Adds two floats.", "{A} + {B}");
        d.pins = { DataIn("A", PinType::Float, "0.0"), DataIn("B", PinType::Float, "0.0"),
                   DataOut("Result", PinType::Float) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.MultiplyFloat", "Multiply (Float)", NodeCategory::Math,
                   "Multiplies two floats.", "{A} * {B}");
        d.pins = { DataIn("A", PinType::Float, "1.0"), DataIn("B", PinType::Float, "1.0"),
                   DataOut("Result", PinType::Float) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.And", "And", NodeCategory::Math,
                   "Logical AND.", "{A} && {B}");
        d.pins = { DataIn("A", PinType::Bool, "True"), DataIn("B", PinType::Bool, "True"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Or", "Or", NodeCategory::Math,
                   "Logical OR.", "{A} || {B}");
        d.pins = { DataIn("A", PinType::Bool, "False"), DataIn("B", PinType::Bool, "False"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Not", "Not", NodeCategory::Math,
                   "Logical NOT.", "!{A}");
        d.pins = { DataIn("A", PinType::Bool, "False"), DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Equal", "Equal", NodeCategory::Math,
                   "Returns true if A == B.", "{A} == {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.NotEqual", "Not Equal", NodeCategory::Math,
                   "Returns true if A != B.", "{A} != {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GreaterThan", "Greater Than", NodeCategory::Math,
                   "Returns true if A > B.", "{A} > {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.LessThan", "Less Than", NodeCategory::Math,
                   "Returns true if A < B.", "{A} < {B}");
        d.pins = { DataIn("A", PinType::Int, "0"), DataIn("B", PinType::Int, "0"),
                   DataOut("Result", PinType::Bool) };
        Reg(std::move(d));
    }

    // ── Debug Nodes ───────────────────────────────────────────────────────────
    {
        auto d = N("builtin.Notification", "Notification", NodeCategory::Debug,
                   "Shows a notification message on screen.",
                   "Debug.Notification({akMessage})");
        d.pins = { ExecIn(), DataIn("akMessage", PinType::String, ""), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.MessageBox", "Message Box", NodeCategory::Debug,
                   "Shows a message box dialog.",
                   "Debug.MessageBox({akMessage})");
        d.pins = { ExecIn(), DataIn("akMessage", PinType::String, ""), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Trace", "Trace", NodeCategory::Debug,
                   "Writes a message to the Papyrus log.",
                   "Debug.Trace({akMessage}, {aiSeverity})");
        d.pins = { ExecIn(), DataIn("akMessage", PinType::String, ""),
                   DataIn("aiSeverity", PinType::Int, "0"), ExecOut() };
        Reg(std::move(d));
    }

    // ── Actor Nodes ───────────────────────────────────────────────────────────
    {
        auto d = N("builtin.GetPlayer", "Get Player", NodeCategory::Actor,
                   "Returns the player actor reference.",
                   "Game.GetPlayer()");
        d.pins = { DataOut("Player", PinType::Actor) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GetFormByID", "Get Form By ID", NodeCategory::Actor,
                   "Returns a Form by its base FormID.",
                   "Game.GetForm({aiFormID})");
        d.pins = { DataIn("aiFormID", PinType::Int, "0"), DataOut("Form", PinType::Form) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.ApplySpell", "Apply Spell", NodeCategory::Actor,
                   "Adds a spell to the target actor.",
                   "{akTarget}.AddSpell({akSpell})");
        d.pins = { ExecIn(), DataIn("akTarget", PinType::Actor),
                   DataIn("akSpell", PinType::Form), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.RemoveSpell", "Remove Spell", NodeCategory::Actor,
                   "Removes a spell from the target actor.",
                   "{akTarget}.RemoveSpell({akSpell})");
        d.pins = { ExecIn(), DataIn("akTarget", PinType::Actor),
                   DataIn("akSpell", PinType::Form), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.IsDead", "Is Dead", NodeCategory::Actor,
                   "Returns true if the actor is dead.",
                   "{akActor}.IsDead()");
        d.pins = { DataIn("akActor", PinType::Actor), DataOut("Dead", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GetLevel", "Get Level", NodeCategory::Actor,
                   "Returns the actor's current level.",
                   "{akActor}.GetLevel()");
        d.pins = { DataIn("akActor", PinType::Actor), DataOut("Level", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Resurrect", "Resurrect", NodeCategory::Actor,
                   "Resurrects a dead actor.",
                   "{akActor}.Resurrect()");
        d.pins = { ExecIn(), DataIn("akActor", PinType::Actor), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GetActorValue", "Get Actor Value", NodeCategory::Actor,
                   "Gets an actor value by name.",
                   "{akActor}.GetActorValue({asAVName})");
        d.pins = { DataIn("akActor", PinType::Actor),
                   DataIn("asAVName", PinType::String, "Health"),
                   DataOut("Value", PinType::Float) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.SetActorValue", "Set Actor Value", NodeCategory::Actor,
                   "Sets an actor value by name.",
                   "{akActor}.SetActorValue({asAVName}, {afValue})");
        d.pins = { ExecIn(), DataIn("akActor", PinType::Actor),
                   DataIn("asAVName", PinType::String, "Health"),
                   DataIn("afValue", PinType::Float, "0.0"), ExecOut() };
        Reg(std::move(d));
    }

    // ── Quest / Story Nodes ───────────────────────────────────────────────────
    {
        auto d = N("builtin.SetStage", "Set Stage", NodeCategory::Quest,
                   "Sets the current stage of a quest.",
                   "{akQuest}.SetStage({aiStage})");
        d.pins = { ExecIn(), DataIn("akQuest", PinType::Quest),
                   DataIn("aiStage", PinType::Int, "0"), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.GetStage", "Get Stage", NodeCategory::Quest,
                   "Gets the current stage of a quest.",
                   "{akQuest}.GetStage()");
        d.pins = { DataIn("akQuest", PinType::Quest), DataOut("Stage", PinType::Int) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.IsStageDone", "Is Stage Done", NodeCategory::Quest,
                   "Returns true if the given quest stage is complete.",
                   "{akQuest}.IsStageDone({aiStage})");
        d.pins = { DataIn("akQuest", PinType::Quest),
                   DataIn("aiStage", PinType::Int, "0"),
                   DataOut("Done", PinType::Bool) };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.CompleteQuest", "Complete Quest", NodeCategory::Quest,
                   "Marks a quest as complete.",
                   "{akQuest}.CompleteQuest()");
        d.pins = { ExecIn(), DataIn("akQuest", PinType::Quest), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.StartQuest", "Start Quest", NodeCategory::Quest,
                   "Starts a quest.",
                   "{akQuest}.Start()");
        d.pins = { ExecIn(), DataIn("akQuest", PinType::Quest), ExecOut() };
        Reg(std::move(d));
    }

    // ── Utility / Timer Nodes ─────────────────────────────────────────────────
    {
        auto d = N("builtin.StartTimer", "Start Timer", NodeCategory::Utility,
                   "Starts a timer that will fire OnTimer after the given interval.",
                   "StartTimer({afInterval}, {aiTimerID})");
        d.pins = { ExecIn(), DataIn("afInterval", PinType::Float, "1.0"),
                   DataIn("aiTimerID", PinType::Int, "1"), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.CancelTimer", "Cancel Timer", NodeCategory::Utility,
                   "Cancels a running timer.",
                   "CancelTimer({aiTimerID})");
        d.pins = { ExecIn(), DataIn("aiTimerID", PinType::Int, "1"), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.StartTimerGameTime", "Start Timer (Game Time)", NodeCategory::Utility,
                   "Starts a timer measured in in-game hours.",
                   "StartTimerGameTime({afInterval}, {aiTimerID})");
        d.pins = { ExecIn(), DataIn("afInterval", PinType::Float, "1.0"),
                   DataIn("aiTimerID", PinType::Int, "1"), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.Wait", "Wait", NodeCategory::Utility,
                   "Pauses script execution for the given number of real-time seconds.",
                   "Utility.Wait({afSeconds})");
        d.pins = { ExecIn(), DataIn("afSeconds", PinType::Float, "1.0"), ExecOut() };
        Reg(std::move(d));
    }
    {
        auto d = N("builtin.RandomInt", "Random Int", NodeCategory::Utility,
                   "Returns a random integer in [aiMin, aiMax].",
                   "Utility.RandomInt({aiMin}, {aiMax})");
        d.pins = { DataIn("aiMin", PinType::Int, "0"), DataIn("aiMax", PinType::Int, "100"),
                   DataOut("Result", PinType::Int) };
        Reg(std::move(d));
    }
    {        auto d = N("builtin.RandomFloat", "Random Float", NodeCategory::Utility,
                   "Returns a random float in [afMin, afMax].",
                   "Utility.RandomFloat({afMin}, {afMax})");
        d.pins = { DataIn("afMin", PinType::Float, "0.0"), DataIn("afMax", PinType::Float, "1.0"),
                   DataOut("Result", PinType::Float) };
        Reg(std::move(d));
    }

    // ── Self ──────────────────────────────────────────────────────────────────
    {
        auto d = N("builtin.GetSelf", "Self", NodeCategory::Variable,
                   "Returns a reference to this script's owner object.",
                   "Self");
        d.pins = { DataOut("Self", PinType::ObjectRef) };
        Reg(std::move(d));
    }
}

// ── Property node synchronisation ─────────────────────────────────────────────

void BuiltinNodes::SyncPropertyNodes(const std::string& script_name,
                                      const std::vector<PropertyDefinition>& props) {
    auto& reg = NodeRegistry::Get();
    const std::string prefix = "script." + script_name + ".";

    // Remove all existing property nodes for this script
    std::vector<std::string> to_remove;
    for (const auto& def : reg.AllNodes())
        if (def.type_id.rfind(prefix, 0) == 0)
            to_remove.push_back(def.type_id);
    for (const auto& id : to_remove)
        reg.Unregister(id);

    // Re-register from current property list
    for (const auto& prop : props) {
        // Get Property node
        {
            NodeDefinition d;
            d.type_id          = prefix + "get." + prop.name;
            d.display_name     = "Get " + prop.name;
            d.category         = NodeCategory::Variable;
            d.tooltip          = "Read property " + prop.name;
            d.codegen_template = prop.name; // bare identifier — no {token} needed
            d.pins             = { DataOut(prop.name.c_str(), prop.type) };
            reg.Register(std::move(d));
        }
        // Set Property node
        {
            NodeDefinition d;
            d.type_id          = prefix + "set." + prop.name;
            d.display_name     = "Set " + prop.name;
            d.category         = NodeCategory::Variable;
            d.tooltip          = "Write property " + prop.name;
            d.codegen_template = prop.name + " = {Value}";
            d.pins             = { ExecIn(), DataIn("Value", prop.type), ExecOut() };
            reg.Register(std::move(d));
        }
    }
}

// ── Function node removal ─────────────────────────────────────────────────────

void BuiltinNodes::RemoveFunctionNodes(const std::string& script_name,
                                        const std::string& func_name) {
    NodeRegistry::Get().Unregister("script." + script_name + ".entry."  + func_name);
    NodeRegistry::Get().Unregister("script." + script_name + ".return." + func_name);
    NodeRegistry::Get().Unregister("script." + script_name + ".call."   + func_name);
}

// ── Cross-script call node synchronisation (task 3.10) ────────────────────────

void BuiltinNodes::SyncCrossScriptNodes(const std::vector<ScriptGraph>& scripts) {
    auto& reg = NodeRegistry::Get();

    // Remove all previously registered cross-script nodes
    std::vector<std::string> to_remove;
    for (const auto& def : reg.AllNodes())
        if (def.type_id.rfind("project.", 0) == 0)
            to_remove.push_back(def.type_id);
    for (const auto& id : to_remove)
        reg.Unregister(id);

    // Re-register one call node per function per script
    for (const auto& script : scripts) {
        for (const auto& func : script.functions) {
            NodeDefinition d;
            d.type_id      = "project." + script.script_name + "." + func.name;
            d.display_name = script.script_name + "::" + func.name;
            d.category     = NodeCategory::Custom;
            d.tooltip      = "Call " + func.name + " on a " + script.script_name + " reference";

            // Build codegen template: ({Self} as ScriptName).FuncName({p0}, {p1}, ...)
            std::string tmpl = "({Self} as " + script.script_name + ")." + func.name + "(";
            for (size_t i = 0; i < func.parameters.size(); ++i) {
                if (i) tmpl += ", ";
                tmpl += "{" + func.parameters[i].name + "}";
            }
            tmpl += ")";
            d.codegen_template = tmpl;

            // Pins
            PinDefinition exec_in;
            exec_in.name = "In"; exec_in.kind = PinKind::Input;
            exec_in.flow = PinFlow::Execution; exec_in.type = PinType::Exec;
            d.pins.push_back(exec_in);

            PinDefinition self_pin;
            self_pin.name = "Self"; self_pin.kind = PinKind::Input;
            self_pin.flow = PinFlow::Data; self_pin.type = PinType::ObjectRef;
            self_pin.tooltip = "Reference of type " + script.script_name;
            d.pins.push_back(self_pin);

            for (const auto& param : func.parameters) {
                PinDefinition p = param;
                p.kind = PinKind::Input; p.flow = PinFlow::Data;
                d.pins.push_back(p);
            }

            PinDefinition exec_out;
            exec_out.name = "Out"; exec_out.kind = PinKind::Output;
            exec_out.flow = PinFlow::Execution; exec_out.type = PinType::Exec;
            d.pins.push_back(exec_out);

            if (func.return_type != PinType::Unknown) {
                PinDefinition ret;
                ret.name = "ReturnValue"; ret.kind = PinKind::Output;
                ret.flow = PinFlow::Data; ret.type = func.return_type;
                d.pins.push_back(ret);
            }

            reg.Register(std::move(d));
        }
    }
}

} // namespace graph
