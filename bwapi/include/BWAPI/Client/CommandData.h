#pragma once

#include "UnitCommand.h"
#include "Command.h"
#include "Shape.h"

namespace BWAPI
{
  /// Everything the client writes and the server reads.
  ///
  /// This used to live inside GameData, which is why the client had to map the whole 33 MB state
  /// plane writable to say "move this unit" - defect 2.3 in ADR 0001 section 2. Splitting it out
  /// lets the client map the state read-only and keep write access to exactly the region it is
  /// supposed to write.
  ///
  /// It is also two thirds of the bytes: the string table alone is 20 MB, and none of it was ever
  /// state.
  ///
  /// Read every field here as untrusted. The server clamps and range-checks all of it
  /// (BWAPI/Source/BWAPI/ClientInput.h) because a read-only state plane raises the cost of
  /// tampering with this one and does not remove it: the section is named after the server's
  /// process id and a client opens it by name.
  struct CommandData
  {
    static const int MAX_STRINGS      = 20000;
    static const int MAX_SHAPES       = 20000;
    static const int MAX_COMMANDS     = 20000;
    static const int MAX_UNIT_COMMANDS = 20000;

    //strings (used in shapes and commands)
    int stringCount;
    char strings[MAX_STRINGS][1024];

    //shapes, commands, unitCommands, from client to server
    int shapeCount;
    BWAPIC::Shape shapes[MAX_SHAPES];

    int commandCount;
    BWAPIC::Command commands[MAX_COMMANDS];

    int unitCommandCount;
    BWAPIC::UnitCommand unitCommands[MAX_UNIT_COMMANDS];

    // The instants that bracket the client's own work on a frame, so the server can charge the
    // bot for its work rather than for the handover (ADR 0001 section 2, defect 2.5).
    long long clientWakeMicros;
    long long clientReplyMicros;
  };
}
