
/*
    Copyright (C) 1999-2008 by Mark D. Hill and David A. Wood for the
    Wisconsin Multifacet Project.  Contact: gems@cs.wisc.edu
    http://www.cs.wisc.edu/gems/

    --------------------------------------------------------------------

    This file is part of the SLICC (Specification Language for
    Implementing Cache Coherence), a component of the Multifacet GEMS
    (General Execution-driven Multiprocessor Simulator) software
    toolset originally developed at the University of Wisconsin-Madison.

    SLICC was originally developed by Milo Martin with substantial
    contributions from Daniel Sorin.

    Substantial further development of Multifacet GEMS at the
    University of Wisconsin was performed by Alaa Alameldeen, Brad
    Beckmann, Jayaram Bobba, Ross Dickson, Dan Gibson, Pacia Harper,
    Derek Hower, Milo Martin, Michael Marty, Carl Mauer, Michelle Moravan,
    Kevin Moore, Andrew Phelps, Manoj Plakal, Daniel Sorin, Haris Volos,
    Min Xu, and Luke Yen.
    --------------------------------------------------------------------

    If your use of this software contributes to a published paper, we
    request that you (1) cite our summary paper that appears on our
    website (http://www.cs.wisc.edu/gems/) and (2) e-mail a citation
    for your published paper to gems@cs.wisc.edu.

    If you redistribute derivatives of this software, we request that
    you notify us and either (1) ask people to register with us at our
    website (http://www.cs.wisc.edu/gems/) or (2) collect registration
    information and periodically send it to us.

    --------------------------------------------------------------------

    Multifacet GEMS is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    Multifacet GEMS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Multifacet GEMS; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307, USA

    The GNU General Public License is contained in the file LICENSE.

### END HEADER ###
*/

/*
 * $Id$
 *
 * */

#include "StateMachine.h"
#include "fileio.h"
#include "html_gen.h"
#include "Action.h"
#include "Event.h"
#include "State.h"
#include "Transition.h"
#include "Var.h"
#include "SymbolTable.h"
#include "util.h"
#include "Vector.h"

StateMachine::StateMachine(string ident, const Location& location, const Map<string, string>& pairs)
  : Symbol(ident, location, pairs)
{
  m_table_built = false;
}

StateMachine::~StateMachine()
{
  // FIXME
  // assert(0);
}

void StateMachine::addState(State* state_ptr)
{
  assert(m_table_built == false);
  m_state_map.add(state_ptr, m_states.size());
  m_states.insertAtBottom(state_ptr);
}

void StateMachine::addEvent(Event* event_ptr)
{
  assert(m_table_built == false);
  m_event_map.add(event_ptr, m_events.size());
  m_events.insertAtBottom(event_ptr);
}

void StateMachine::addAction(Action* action_ptr)
{
  assert(m_table_built == false);

  // Check for duplicate action
  int size = m_actions.size();
  for(int i=0; i<size; i++) {
    if (m_actions[i]->getIdent() == action_ptr->getIdent()) {
      m_actions[i]->warning("Duplicate action definition: " + m_actions[i]->getIdent());
      action_ptr->error("Duplicate action definition: " + action_ptr->getIdent());
    }
    if (m_actions[i]->getShorthand() == action_ptr->getShorthand()) {
      m_actions[i]->warning("Duplicate action shorthand: " + m_actions[i]->getIdent());
      m_actions[i]->warning("    shorthand = " + m_actions[i]->getShorthand());
      action_ptr->warning("Duplicate action shorthand: " + action_ptr->getIdent());
      action_ptr->error("    shorthand = " + action_ptr->getShorthand());
    }
  }

  m_actions.insertAtBottom(action_ptr);
}

void StateMachine::addTransition(Transition* trans_ptr)
{
  assert(m_table_built == false);
  trans_ptr->checkIdents(m_states, m_events, m_actions);
  m_transitions.insertAtBottom(trans_ptr);
}

void StateMachine::addFunc(Func* func_ptr)
{
  // register func in the symbol table
  g_sym_table.registerSym(func_ptr->toString(), func_ptr);
  m_internal_func_vec.insertAtBottom(func_ptr);
}

void StateMachine::buildTable()
{
  assert(m_table_built == false);
  int numStates = m_states.size();
  int numEvents = m_events.size();
  int numTransitions = m_transitions.size();
  int stateIndex, eventIndex;

  for(stateIndex=0; stateIndex < numStates; stateIndex++) {
    m_table.insertAtBottom(Vector<Transition*>());
    for(eventIndex=0; eventIndex < numEvents; eventIndex++) {
      m_table[stateIndex].insertAtBottom(NULL);
    }
  }

  for(int i=0; i<numTransitions; i++) {
    Transition* trans_ptr = m_transitions[i];

    // Track which actions we touch so we know if we use them all --
    // really this should be done for all symbols as part of the
    // symbol table, then only trigger it for Actions, States, Events,
    // etc.

    Vector<Action*> actions = trans_ptr->getActions();
    for(int actionIndex=0; actionIndex < actions.size(); actionIndex++) {
      actions[actionIndex]->markUsed();
    }

    stateIndex = getStateIndex(trans_ptr->getStatePtr());
    eventIndex = getEventIndex(trans_ptr->getEventPtr());
    if (m_table[stateIndex][eventIndex] != NULL) {
      m_table[stateIndex][eventIndex]->warning("Duplicate transition: " + m_table[stateIndex][eventIndex]->toString());
      trans_ptr->error("Duplicate transition: " + trans_ptr->toString());
    }
    m_table[stateIndex][eventIndex] = trans_ptr;
  }

  // Look at all actions to make sure we used them all
  for(int actionIndex=0; actionIndex < m_actions.size(); actionIndex++) {
    Action* action_ptr = m_actions[actionIndex];
    if (!action_ptr->wasUsed()) {
      string error_msg = "Unused action: " +  action_ptr->getIdent();
      if (action_ptr->existPair("desc")) {
        error_msg += ", "  + action_ptr->getDescription();
      }
      action_ptr->warning(error_msg);
    }
  }

  m_table_built = true;
}

const Transition* StateMachine::getTransPtr(int stateIndex, int eventIndex) const
{
  return m_table[stateIndex][eventIndex];
}

// *********************** //
// ******* C Files ******* //
// *********************** //

void StateMachine::writeCFiles(string path) const
{
  string comp = getIdent();
  string filename;

  // Output switch statement for transition table
  {
    ostringstream sstr;
    printCSwitch(sstr, comp);
    conditionally_write_file(path + comp + "_Transitions.C", sstr);
  }

  // Output the actions for performing the actions
  {
    ostringstream sstr;
    printControllerC(sstr, comp);
    conditionally_write_file(path + comp + "_Controller.C", sstr);
  }

  // Output the method declarations for the class declaration
  {
    ostringstream sstr;
    printControllerH(sstr, comp);
    conditionally_write_file(path + comp + "_Controller.h", sstr);
  }

  // Output the wakeup loop for the events
  {
    ostringstream sstr;
    printCWakeup(sstr, comp);
    conditionally_write_file(path + comp + "_Wakeup.C", sstr);
  }

  // Profiling
  {
    ostringstream sstr;
    printProfilerC(sstr, comp);
    conditionally_write_file(path + comp + "_Profiler.C", sstr);
  }
  {
    ostringstream sstr;
    printProfilerH(sstr, comp);
    conditionally_write_file(path + comp + "_Profiler.h", sstr);
  }

  // Write internal func files
  for(int i=0; i<m_internal_func_vec.size(); i++) {
    m_internal_func_vec[i]->writeCFiles(path);
  }

}

void StateMachine::printControllerH(ostream& out, string component) const
{
  out << "/** \\file " << getIdent() << ".h" << endl;
  out << "  * " << endl;
  out << "  * Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "  * Created by slicc definition of Module \"" << getShorthand() << "\"" << endl;
  out << "  */" << endl;
  out << endl;
  out << "#ifndef " << component << "_CONTROLLER_H" << endl;
  out << "#define " << component << "_CONTROLLER_H" << endl;
  out << endl;
  out << "#include \"Global.h\"" << endl;
  out << "#include \"Consumer.h\"" << endl;
  out << "#include \"TransitionResult.h\"" << endl;
  out << "#include \"Types.h\"" << endl;
  out << "#include \"" << component << "_Profiler.h\"" << endl;
  out << endl;

  // for adding information to the protocol debug trace
  out << "extern stringstream " << component << "_" << "transitionComment;" << endl;

  out << "class " << component << "_Controller : public Consumer {" << endl;

  /* the coherence checker needs to call isBlockExclusive() and isBlockShared()
     making the Chip a friend class is an easy way to do this for now */
  out << "#ifdef CHECK_COHERENCE" << endl;
  out << "  friend class Chip;" << endl;
  out << "#endif /* CHECK_COHERENCE */" << endl;

  out << "public:" << endl;
  out << "  " << component << "_Controller(Chip* chip_ptr, int version);" << endl;
  out << "  void print(ostream& out) const;" << endl;
  out << "  void wakeup();" << endl;
  out << "  static void dumpStats(ostream& out) { s_profiler.dumpStats(out); }" << endl;
  out << "  static void clearStats() { s_profiler.clearStats(); }" << endl;
  out << "public:" << endl;
  out << "  TransitionResult doTransition(" << component << "_Event event, " << component
      << "_State state, const Address& addr";
  if(CHECK_INVALID_RESOURCE_STALLS) {
    out << ", int priority";
  }
  out << ");  // in " << component << "_Transitions.C" << endl;
  out << "  TransitionResult doTransitionWorker(" << component << "_Event event, " << component
      << "_State state, " <<  component << "_State& next_state, const Address& addr";
  if(CHECK_INVALID_RESOURCE_STALLS) {
    out << ", int priority";
  }
  out << ");  // in " << component << "_Transitions.C" << endl;
  out << "  Chip* m_chip_ptr;" << endl;
  out << "  NodeID m_id;" << endl;
  out << "  NodeID m_version;" << endl;
  out << "  MachineID m_machineID;" << endl;
  out << "  static " << component << "_Profiler s_profiler;" << endl;

  // internal function protypes
  out << "  // Internal functions" << endl;
  for(int i=0; i<m_internal_func_vec.size(); i++) {
    Func* func = m_internal_func_vec[i];
    string proto;
    func->funcPrototype(proto);
    if (proto != "") {
      out << "  " << proto;
    }
  }

  out << "  // Actions" << endl;
  for(int i=0; i < numActions(); i++) {
    const Action& action = getAction(i);
    out << "/** \\brief " << action.getDescription() << "*/" << endl;
    out << "  void " << action.getIdent() << "(const Address& addr);" << endl;
  }
  out << "};" << endl;
  out << "#endif // " << component << "_CONTROLLER_H" << endl;
}

void StateMachine::printControllerC(ostream& out, string component) const
{
  out << "/** \\file " << getIdent() << ".C" << endl;
  out << "  * " << endl;
  out << "  * Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "  * Created by slicc definition of Module \"" << getShorthand() << "\"" << endl;
  out << "  */" << endl;
  out << endl;
  out << "#include \"Global.h\"" << endl;
  out << "#include \"RubySlicc_includes.h\"" << endl;
  out << "#include \"" << component << "_Controller.h\"" << endl;
  out << "#include \"" << component << "_State.h\"" << endl;
  out << "#include \"" << component << "_Event.h\"" << endl;
  out << "#include \"Types.h\"" << endl;
  out << "#include \"System.h\"" << endl;
  out << "#include \"Chip.h\"" << endl;
  out << endl;

  // for adding information to the protocol debug trace
  out << "stringstream " << component << "_" << "transitionComment;" << endl;
  out << "#define APPEND_TRANSITION_COMMENT(str) (" << component << "_" << "transitionComment << str)" << endl;

  out << "/** \\brief static profiler defn */" << endl;
  out << component << "_Profiler " << component << "_Controller::s_profiler;" << endl;
  out << endl;

  out << "/** \\brief constructor */" << endl;
  out << component << "_Controller::" << component
      << "_Controller(Chip* chip_ptr, int version)" << endl;
  out << "{" << endl;
  out << "  m_chip_ptr = chip_ptr;" << endl;
  out << "  m_id = m_chip_ptr->getID();" << endl;
  out << "  m_version = version;" << endl;
  out << "  m_machineID.type = MachineType_" << component << ";" << endl;
  out << "  m_machineID.num = m_id*RubyConfig::numberOf"<< component << "PerChip()+m_version;" << endl;

  // Set the queue consumers
  for(int i=0; i < m_in_ports.size(); i++) {
    const Var* port = m_in_ports[i];
    out << "  " << port->getCode() << ".setConsumer(this);" << endl;
  }

  out << endl;
  // Set the queue descriptions
  for(int i=0; i < m_in_ports.size(); i++) {
    const Var* port = m_in_ports[i];
    out << "  " << port->getCode()
        << ".setDescription(\"[Chip \" + int_to_string(m_chip_ptr->getID()) + \" \" + int_to_string(m_version) + \", "
        << component << ", " << port->toString() << "]\");" << endl;
  }

  // Initialize the transition profiling
  out << endl;
  for(int i=0; i<numTransitions(); i++) {
    const Transition& t = getTransition(i);
    const Vector<Action*>& action_vec = t.getActions();
    int numActions = action_vec.size();

    // Figure out if we stall
    bool stall = false;
    for (int i=0; i<numActions; i++) {
      if(action_vec[i]->getIdent() == "z_stall") {
        stall = true;
      }
    }

    // Only possible if it is not a 'z' case
    if (!stall) {
      out << "  s_profiler.possibleTransition(" << component << "_State_"
          << t.getStatePtr()->getIdent() << ", " << component << "_Event_"
          << t.getEventPtr()->getIdent() << ");" << endl;
    }
  }

  out << "}" << endl;

  out << endl;

  out << "void " << component << "_Controller::print(ostream& out) const { out << \"[" << component
      << "_Controller \" << m_chip_ptr->getID() << \" \" << m_version << \"]\"; }" << endl;

  out << endl;
  out << "// Actions" << endl;
  out << endl;

  for(int i=0; i < numActions(); i++) {
    const Action& action = getAction(i);
    if (action.existPair("c_code")) {
      out << "/** \\brief " << action.getDescription() << "*/" << endl;
      out << "void " << component << "_Controller::"
          << action.getIdent() << "(const Address& addr)" << endl;
      out << "{" << endl;
      out << "  DEBUG_MSG(GENERATED_COMP, HighPrio,\"executing\");" << endl;
      out << action.lookupPair("c_code");
      out << "}" << endl;
    }
    out << endl;
  }
}

void StateMachine::printCWakeup(ostream& out, string component) const
{
  out << "// Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "// " << getIdent() << ": " << getShorthand() << endl;
  out << endl;
  out << "#include \"Global.h\"" << endl;
  out << "#include \"RubySlicc_includes.h\"" << endl;
  out << "#include \"" << component << "_Controller.h\"" << endl;
  out << "#include \"" << component << "_State.h\"" << endl;
  out << "#include \"" << component << "_Event.h\"" << endl;
  out << "#include \"Types.h\"" << endl;
  out << "#include \"System.h\"" << endl;
  out << "#include \"Chip.h\"" << endl;
  out << endl;
  out << "void " << component << "_Controller::wakeup()" << endl;
  out << "{" << endl;
  //  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,*this);" << endl;
  //  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,g_eventQueue_ptr->getTime());" << endl;
  out << endl;
  out << "int counter = 0;" << endl;
  out << "  while (true) {" << endl;
  out << "    // Some cases will put us into an infinite loop without this limit" << endl;
  out << "    assert(counter <= RubyConfig::" << getIdent() << "TransitionsPerCycle());" << endl;
  out << "    if (counter == RubyConfig::" << getIdent() << "TransitionsPerCycle()) {" << endl;
  out << "      g_system_ptr->getProfiler()->controllerBusy(m_machineID); // Count how often we're fully utilized" << endl;
  out << "      g_eventQueue_ptr->scheduleEvent(this, 1); // Wakeup in another cycle and try again" << endl;
  out << "      break;" << endl;
  out << "    }" << endl;

  // InPorts
  for(int i=0; i < m_in_ports.size(); i++) {
    const Var* port = m_in_ports[i];
    assert(port->existPair("c_code_in_port"));
    out << "    // "
        << component << "InPort " << port->toString()
        << endl;
    out << port->lookupPair("c_code_in_port");
    out << endl;
  }

  out << "    break;  // If we got this far, we have nothing left todo" << endl;
  out << "  }" << endl;
  //  out << "  g_eventQueue_ptr->scheduleEvent(this, 1);" << endl;
  //  out << "  DEBUG_NEWLINE(GENERATED_COMP, MedPrio);" << endl;
  out << "}" << endl;
  out << endl;
}

void StateMachine::printCSwitch(ostream& out, string component) const
{
  out << "// Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "// " << getIdent() << ": " << getShorthand() << endl;
  out << endl;
  out << "#include \"Global.h\"" << endl;
  out << "#include \"" << component << "_Controller.h\"" << endl;
  out << "#include \"" << component << "_State.h\"" << endl;
  out << "#include \"" << component << "_Event.h\"" << endl;
  out << "#include \"Types.h\"" << endl;
  out << "#include \"System.h\"" << endl;
  out << "#include \"Chip.h\"" << endl;
  out << endl;
  out << "#define HASH_FUN(state, event)  ((int(state)*" << component
      << "_Event_NUM)+int(event))" << endl;
  out << endl;
  out << "#define GET_TRANSITION_COMMENT() (" << component << "_" << "transitionComment.str())" << endl;
  out << "#define CLEAR_TRANSITION_COMMENT() (" << component << "_" << "transitionComment.str(\"\"))" << endl;
  out << endl;
  out << "TransitionResult " << component << "_Controller::doTransition("
      << component << "_Event event, "
      << component << "_State state, "
      << "const Address& addr" << endl;
  if(CHECK_INVALID_RESOURCE_STALLS) {
    out << ", int priority";
  }
  out << ")" << endl;

  out << "{" << endl;
  out << "  " << component << "_State next_state = state;" << endl;
  out << endl;
  out << "  DEBUG_NEWLINE(GENERATED_COMP, MedPrio);" << endl;
  out << "  DEBUG_MSG(GENERATED_COMP, MedPrio,*this);" << endl;
  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,g_eventQueue_ptr->getTime());" << endl;
  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,state);" << endl;
  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,event);" << endl;
  out << "  DEBUG_EXPR(GENERATED_COMP, MedPrio,addr);" << endl;
  out << endl;
  out << "  TransitionResult result = doTransitionWorker(event, state, next_state, addr";
  if(CHECK_INVALID_RESOURCE_STALLS) {
    out << ", priority";
  }
  out << ");" << endl;
  out << endl;
  out << "  if (result == TransitionResult_Valid) {" << endl;
  out << "    DEBUG_EXPR(GENERATED_COMP, MedPrio, next_state);" << endl;
  out << "    DEBUG_NEWLINE(GENERATED_COMP, MedPrio);" << endl;
  out << "    s_profiler.countTransition(state, event);" << endl;
  out << "    if (PROTOCOL_DEBUG_TRACE) {" << endl
      << "      g_system_ptr->getProfiler()->profileTransition(\"" << component
      << "\", m_chip_ptr->getID(), m_version, addr, " << endl
      << "        " << component << "_State_to_string(state), " << endl
      << "        " << component << "_Event_to_string(event), " << endl
      << "        " << component << "_State_to_string(next_state), GET_TRANSITION_COMMENT());" << endl
      << "    }" << endl;
  out << "    CLEAR_TRANSITION_COMMENT();" << endl;
  out << "    " << component << "_setState(addr, next_state);" << endl;
  out << "    " << endl;
  out << "  } else if (result == TransitionResult_ResourceStall) {" << endl;
  out << "    if (PROTOCOL_DEBUG_TRACE) {" << endl
      << "      g_system_ptr->getProfiler()->profileTransition(\"" << component
      << "\", m_chip_ptr->getID(), m_version, addr, " << endl
      << "        " << component << "_State_to_string(state), " << endl
      << "        " << component << "_Event_to_string(event), " << endl
      << "        " << component << "_State_to_string(next_state), " << endl
      << "        \"Resource Stall\");" << endl
      << "    }" << endl;
  out << "  } else if (result == TransitionResult_ProtocolStall) {" << endl;
  out << "    DEBUG_MSG(GENERATED_COMP,HighPrio,\"stalling\");" << endl
      << "    DEBUG_NEWLINE(GENERATED_COMP, MedPrio);" << endl;
  out << "    if (PROTOCOL_DEBUG_TRACE) {" << endl
      << "      g_system_ptr->getProfiler()->profileTransition(\"" << component
      << "\", m_chip_ptr->getID(), m_version, addr, " << endl
      << "        " << component << "_State_to_string(state), " << endl
      << "        " << component << "_Event_to_string(event), " << endl
      << "        " << component << "_State_to_string(next_state), " << endl
      << "        \"Protocol Stall\");" << endl
      << "    }" << endl
      << "  }" << endl;
  out << "  return result;" << endl;
  out << "}" << endl;
  out << endl;
  out << "TransitionResult " << component << "_Controller::doTransitionWorker("
      << component << "_Event event, "
      << component << "_State state, "
      << component << "_State& next_state, "
      << "const Address& addr" << endl;
  if(CHECK_INVALID_RESOURCE_STALLS) {
    out << ", int priority" << endl;
  }
  out << ")" << endl;

  out << "{" << endl;
  out << "" << endl;

  out << "  switch(HASH_FUN(state, event)) {" << endl;

  Map<string, Vector<string> > code_map; // This map will allow suppress generating duplicate code
  Vector<string> code_vec;

  for(int i=0; i<numTransitions(); i++) {
    const Transition& t = getTransition(i);
    string case_string = component + "_State_" + t.getStatePtr()->getIdent()
      + ", " + component + "_Event_" + t.getEventPtr()->getIdent();

    string code;

    code += "  {\n";
    // Only set next_state if it changes
    if (t.getStatePtr() != t.getNextStatePtr()) {
      code += "    next_state = " + component + "_State_" + t.getNextStatePtr()->getIdent() + ";\n";
    }

    const Vector<Action*>& action_vec = t.getActions();
    int numActions = action_vec.size();

    // Check for resources
    Vector<string> code_sorter;
    const Map<Var*, string>& res = t.getResources();
    Vector<Var*> res_keys = res.keys();
    for (int i=0; i<res_keys.size(); i++) {
      string temp_code;
      if (res_keys[i]->getType()->cIdent() == "DNUCAStopTable") {
        temp_code += res.lookup(res_keys[i]);
      } else {
        temp_code += "    if (!" + (res_keys[i]->getCode()) + ".areNSlotsAvailable(" + res.lookup(res_keys[i]) + ")) {\n";
        if(CHECK_INVALID_RESOURCE_STALLS) {
          // assert that the resource stall is for a resource of equal or greater priority
          temp_code += "      assert(priority >= "+ (res_keys[i]->getCode()) + ".getPriority());\n";
        }
        temp_code += "      return TransitionResult_ResourceStall;\n";
        temp_code += "    }\n";
      }
      code_sorter.insertAtBottom(temp_code);
    }

    // Emit the code sequences in a sorted order.  This makes the
    // output deterministic (without this the output order can vary
    // since Map's keys() on a vector of pointers is not deterministic
    code_sorter.sortVector();
    for (int i=0; i<code_sorter.size(); i++) {
      code += code_sorter[i];
    }

    // Figure out if we stall
    bool stall = false;
    for (int i=0; i<numActions; i++) {
      if(action_vec[i]->getIdent() == "z_stall") {
        stall = true;
      }
    }

    if (stall) {
      code += "    return TransitionResult_ProtocolStall;\n";
    } else {
      for (int i=0; i<numActions; i++) {
        code += "    " + action_vec[i]->getIdent() + "(addr);\n";
      }
      code += "    return TransitionResult_Valid;\n";
    }
    code += "  }\n";


    // Look to see if this transition code is unique.
    if (code_map.exist(code)) {
      code_map.lookup(code).insertAtBottom(case_string);
    } else {
      Vector<string> vec;
      vec.insertAtBottom(case_string);
      code_map.add(code, vec);
      code_vec.insertAtBottom(code);
    }
  }

  // Walk through all of the unique code blocks and spit out the
  // corresponding case statement elements
  for (int i=0; i<code_vec.size(); i++) {
    string code = code_vec[i];

    // Iterative over all the multiple transitions that share the same code
    for (int case_num=0; case_num<code_map.lookup(code).size(); case_num++) {
      string case_string = code_map.lookup(code)[case_num];
      out << "  case HASH_FUN(" << case_string << "):" << endl;
    }
    out << code;
  }

  out << "  default:" << endl;
  out << "    WARN_EXPR(m_id);" << endl;
  out << "    WARN_EXPR(m_version);" << endl;
  out << "    WARN_EXPR(g_eventQueue_ptr->getTime());" << endl;
  out << "    WARN_EXPR(addr);" << endl;
  out << "    WARN_EXPR(event);" << endl;
  out << "    WARN_EXPR(state);" << endl;
  out << "    ERROR_MSG(\"Invalid transition\");" << endl;
  out << "  }" << endl;
  out << "  return TransitionResult_Valid;" << endl;
  out << "}" << endl;
}

void StateMachine::printProfilerH(ostream& out, string component) const
{
  out << "// Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "// " << getIdent() << ": " << getShorthand() << endl;
  out << endl;
  out << "#ifndef " << component << "_PROFILER_H" << endl;
  out << "#define " << component << "_PROFILER_H" << endl;
  out << endl;
  out << "#include \"Global.h\"" << endl;
  out << "#include \"" << component << "_State.h\"" << endl;
  out << "#include \"" << component << "_Event.h\"" << endl;
  out << endl;
  out << "class " << component << "_Profiler {" << endl;
  out << "public:" << endl;
  out << "  " << component << "_Profiler();" << endl;
  out << "  void countTransition(" << component << "_State state, " << component << "_Event event);" << endl;
  out << "  void possibleTransition(" << component << "_State state, " << component << "_Event event);" << endl;
  out << "  void dumpStats(ostream& out) const;" << endl;
  out << "  void clearStats();" << endl;
  out << "private:" << endl;
  out << "  int m_counters[" << component << "_State_NUM][" << component << "_Event_NUM];" << endl;
  out << "  int m_event_counters[" << component << "_Event_NUM];" << endl;
  out << "  bool m_possible[" << component << "_State_NUM][" << component << "_Event_NUM];" << endl;
  out << "};" << endl;
  out << "#endif // " << component << "_PROFILER_H" << endl;
}

void StateMachine::printProfilerC(ostream& out, string component) const
{
  out << "// Auto generated C++ code started by "<<__FILE__<<":"<<__LINE__<< endl;
  out << "// " << getIdent() << ": " << getShorthand() << endl;
  out << endl;
  out << "#include \"" << component << "_Profiler.h\"" << endl;
  out << endl;

  // Constructor
  out << component << "_Profiler::" << component << "_Profiler()" << endl;
  out << "{" << endl;
  out << "  for (int state = 0; state < " << component << "_State_NUM; state++) {" << endl;
  out << "    for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "      m_possible[state][event] = false;" << endl;
  out << "      m_counters[state][event] = 0;" << endl;
  out << "    }" << endl;
  out << "  }" << endl;
  out << "  for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "    m_event_counters[event] = 0;" << endl;
  out << "  }" << endl;
  out << "}" << endl;

  // Clearstats
  out << "void " << component << "_Profiler::clearStats()" << endl;
  out << "{" << endl;
  out << "  for (int state = 0; state < " << component << "_State_NUM; state++) {" << endl;
  out << "    for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "      m_counters[state][event] = 0;" << endl;
  out << "    }" << endl;
  out << "  }" << endl;
  out << "  for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "    m_event_counters[event] = 0;" << endl;
  out << "  }" << endl;
  out << "}" << endl;

  // Count Transition
  out << "void " << component << "_Profiler::countTransition(" << component << "_State state, " << component << "_Event event)" << endl;
  out << "{" << endl;
  out << "  assert(m_possible[state][event]);" << endl;
  out << "  m_counters[state][event]++;" << endl;
  out << "  m_event_counters[event]++;" << endl;
  out << "}" << endl;

  // Possible Transition
  out << "void " << component << "_Profiler::possibleTransition(" << component << "_State state, " << component << "_Event event)" << endl;
  out << "{" << endl;
  out << "  m_possible[state][event] = true;" << endl;
  out << "}" << endl;

  // dumpStats
  out << "void " << component << "_Profiler::dumpStats(ostream& out) const" << endl;
  out << "{" << endl;
  out << "  out << \" --- " << component << " ---\" << endl;" << endl;
  out << "  out << \" - Event Counts -\" << endl;" << endl;
  out << "  for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "    int count = m_event_counters[event];" << endl;
  out << "    out << (" << component << "_Event) event << \"  \" << count << endl;" << endl;
  out << "  }" << endl;
  out << "  out << endl;" << endl;
  out << "  out << \" - Transitions -\" << endl;" << endl;
  out << "  for (int state = 0; state < " << component << "_State_NUM; state++) {" << endl;
  out << "    for (int event = 0; event < " << component << "_Event_NUM; event++) {" << endl;
  out << "      if (m_possible[state][event]) {" << endl;
  out << "        int count = m_counters[state][event];" << endl;
  out << "        out << (" << component << "_State) state << \"  \" << (" << component << "_Event) event << \"  \" << count;" << endl;
  out << "        if (count == 0) {" << endl;
  out << "            out << \" <-- \";" << endl;
  out << "        }" << endl;
  out << "        out << endl;" << endl;
  out << "      }" << endl;
  out << "    }" << endl;
  out << "    out << endl;" << endl;
  out << "  }" << endl;
  out << "}" << endl;
}



// ************************** //
// ******* HTML Files ******* //
// ************************** //

string frameRef(string click_href, string click_target, string over_href, string over_target_num, string text)
{
  string temp;
  temp += "<A href=\"" + click_href + "\" ";
  temp += "target=\"" + click_target + "\" ";
  string javascript = "if (parent.frames[" + over_target_num + "].location != parent.location + '" + over_href + "') { parent.frames[" + over_target_num + "].location='" + over_href + "' }";
  //  string javascript = "parent." + target + ".location='" + href + "'";
  temp += "onMouseOver=\"" + javascript + "\" ";
  temp += ">" + text + "</A>";
  return temp;
}

string frameRef(string href, string target, string target_num, string text)
{
  return frameRef(href, target, href, target_num, text);
}


void StateMachine::writeHTMLFiles(string path) const
{
  string filename;
  string component = getIdent();

  /*
  {
    ostringstream out;
    out << "<html>" << endl;
    out << "<head>" << endl;
    out << "<title>" << component << "</title>" << endl;
    out << "</head>" << endl;
    out << "<frameset rows=\"30,30,*\" frameborder=\"1\">" << endl;
    out << "  <frame name=\"Status\" src=\"empty.html\" marginheight=\"1\">" << endl;
    out << "  <frame name=\"Table\" src=\"" << component << "_table.html\" marginheight=\"1\">" << endl;
    out << "</frameset>" << endl;
    out << "</html>" << endl;
    conditionally_write_file(path + component + ".html", out);
  }
  */

  // Create table with no row hilighted
  {
    ostringstream out;
    printHTMLTransitions(out, numStates()+1);

    // -- Write file
    filename = component + "_table.html";
    conditionally_write_file(path + filename, out);
  }

  // Generate transition tables
  for(int i=0; i<numStates(); i++) {
    ostringstream out;
    printHTMLTransitions(out, i);

    // -- Write file
    filename = component + "_table_" + getState(i).getIdent() + ".html";
    conditionally_write_file(path + filename, out);
  }

  // Generate action descriptions
  for(int i=0; i<numActions(); i++) {
    ostringstream out;
    createHTMLSymbol(getAction(i), "Action", out);

    // -- Write file
    filename = component + "_action_" + getAction(i).getIdent() + ".html";
    conditionally_write_file(path + filename, out);
  }

  // Generate state descriptions
  for(int i=0; i<numStates(); i++) {
    ostringstream out;
    createHTMLSymbol(getState(i), "State", out);

    // -- Write file
    filename = component + "_State_" + getState(i).getIdent() + ".html";
    conditionally_write_file(path + filename, out);
  }

  // Generate event descriptions
  for(int i=0; i<numEvents(); i++) {
    ostringstream out;
    createHTMLSymbol(getEvent(i), "Event", out);

    // -- Write file
    filename = component + "_Event_" + getEvent(i).getIdent() + ".html";
    conditionally_write_file(path + filename, out);
  }
}

void StateMachine::printHTMLTransitions(ostream& out, int active_state) const
{
  // -- Prolog
  out << "<HTML><BODY link=\"blue\" vlink=\"blue\">" << endl;

  // -- Header
  out << "<H1 align=\"center\">" << formatHTMLShorthand(getShorthand()) << ": " << endl;
  Vector<StateMachine*> machine_vec = g_sym_table.getStateMachines();
  for (int i=0; i<machine_vec.size(); i++) {
    StateMachine* type = machine_vec[i];
    if (i != 0) {
      out << " - ";
    }
    if (type == this) {
      out << type->getIdent() << endl;
    } else {
      out << "<A target=\"Table\"href=\"" + type->getIdent() + "_table.html\">" + type->getIdent() + "</A>  " << endl;
    }
  }
  out << "</H1>" << endl;

  // -- Table header
  out << "<TABLE border=1>" << endl;

  // -- Column headers
  out << "<TR>" << endl;

  // -- First column header
  out << "  <TH> </TH>" << endl;

  for(int event = 0; event < numEvents(); event++ ) {
    out << "  <TH bgcolor=white>";
    out << frameRef(getIdent() + "_Event_" + getEvent(event).getIdent() + ".html", "Status", "1", formatHTMLShorthand(getEvent(event).getShorthand()));
    out << "</TH>" << endl;
  }

  out << "</TR>" << endl;

  // -- Body of table
  for(int state = 0; state < numStates(); state++ ) {
    out << "<TR>" << endl;

    // -- Each row
    if (state == active_state) {
      out << "  <TH bgcolor=yellow>";
    } else {
      out << "  <TH bgcolor=white>";
    }

    string click_href = getIdent() + "_table_" + getState(state).getIdent() + ".html";
    string text = formatHTMLShorthand(getState(state).getShorthand());

    out << frameRef(click_href, "Table", getIdent() + "_State_" + getState(state).getIdent() + ".html", "1", formatHTMLShorthand(getState(state).getShorthand()));
    out << "</TH>" << endl;

    // -- One column for each event
    for(int event = 0; event < numEvents(); event++ ) {
      const Transition* trans_ptr = getTransPtr(state, event);

      if( trans_ptr != NULL ) {
        bool stall_action = false;
        string nextState;
        string actions_str;

        // -- Get the actions
        //        actions = trans_ptr->getActionShorthands();
        const Vector<Action*> actions = trans_ptr->getActions();
        for (int action=0; action < actions.size(); action++) {
          if ((actions[action]->getIdent() == "z_stall") ||
              (actions[action]->getIdent() == "zz_recycleMandatoryQueue")) {
            stall_action = true;
          }
          actions_str += "  ";
          actions_str += frameRef(getIdent() + "_action_" + actions[action]->getIdent() + ".html", "Status", "1",
                                  formatHTMLShorthand(actions[action]->getShorthand()));
          actions_str += "\n";
        }

        // -- Get the next state
        if (trans_ptr->getNextStatePtr()->getIdent() != getState(state).getIdent()) {
          string click_href = getIdent() + "_table_" + trans_ptr->getNextStatePtr()->getIdent() + ".html";
          nextState = frameRef(click_href, "Table", getIdent() + "_State_" + trans_ptr->getNextStatePtr()->getIdent() + ".html", "1",
                               formatHTMLShorthand(trans_ptr->getNextStateShorthand()));
        } else {
          nextState = "";
        }

        // -- Print out "actions/next-state"
        if (stall_action) {
          if (state == active_state) {
            out << "  <TD bgcolor=#C0C000>";
          } else {
            out << "  <TD bgcolor=lightgrey>";
          }
        } else if (active_state < numStates() && (trans_ptr->getNextStatePtr()->getIdent() == getState(active_state).getIdent())) {
          out << "  <TD bgcolor=aqua>";
        } else if (state == active_state) {
           out << "  <TD bgcolor=yellow>";
        } else {
          out << "  <TD bgcolor=white>";
        }

        out << actions_str;
        if ((nextState.length() != 0) && (actions_str.length() != 0)) {
          out << "/";
        }
        out << nextState;
        out << "</TD>" << endl;
      } else {
        // This is the no transition case
        if (state == active_state) {
          out << "  <TD bgcolor=#C0C000>&nbsp;</TD>" << endl;
        } else {
          out << "  <TD bgcolor=lightgrey>&nbsp;</TD>" << endl;
        }
      }
    }
    // -- Each row
    if (state == active_state) {
      out << "  <TH bgcolor=yellow>";
    } else {
      out << "  <TH bgcolor=white>";
    }

    click_href = getIdent() + "_table_" + getState(state).getIdent() + ".html";
    text = formatHTMLShorthand(getState(state).getShorthand());

    out << frameRef(click_href, "Table", getIdent() + "_State_" + getState(state).getIdent() + ".html", "1", formatHTMLShorthand(getState(state).getShorthand()));
    out << "</TH>" << endl;

    out << "</TR>" << endl;
  }

  // -- Column footer
  out << "<TR>" << endl;
  out << "  <TH> </TH>" << endl;

  for(int i = 0; i < numEvents(); i++ ) {
    out << "  <TH bgcolor=white>";
    out << frameRef(getIdent() + "_Event_" + getEvent(i).getIdent() + ".html", "Status", "1", formatHTMLShorthand(getEvent(i).getShorthand()));
    out << "</TH>" << endl;
  }
  out << "</TR>" << endl;

  // -- Epilog
  out << "</TABLE>" << endl;
  out << "</BODY></HTML>" << endl;
}


