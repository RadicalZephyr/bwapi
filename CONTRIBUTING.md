# Reporting an Issue
 - Report all issues
 - Trivial documentation issues welcome ([example](https://github.com/bwapi/bwapi/issues/616))
 - Non-trivial feature requests welcome ([example](https://github.com/bwapi/bwapi/issues/393))

# Creating a Pull Request
 - Do development on the [`develop`](https://github.com/bwapi/bwapi/tree/develop) branch.

# Architecture decision records

*This section, and `docs/decisions/`, are additions made by the
`RadicalZephyr/bwapi` fork. Upstream does not carry them.*

Decisions whose *reasoning* is the valuable part get a record in
[`docs/decisions/records/`](docs/decisions/records/). Records are living
documents -- edited to stay current, with new information added as a dated note
marked as arriving after the decision, never as a silent revision of the
original reasoning. Read the directory as the current state of the fork's
decisions, and reach for `git log -p` when you want to know how it got there.
[`docs/decisions/README.md`](docs/decisions/README.md) has the conventions;
[`0001-recording-important-decisions.md`](docs/decisions/records/0001-recording-important-decisions.md)
argues for them.

A record's status is a dated transition log in a collapsed block at the top of
the file, and each row is a transition you log deliberately:

| When | Row you add |
| --- | --- |
| writing the record | `Drafted` |
| a commit before its pull request merges | `Accepted` |
| the pull request that finishes the work | `Implemented` |
| a commit before a superseding record merges | `Superseded by NNNN` |
| when a reversal is written into the record | `Deprecated` |

`Superseded` points outward, to the record that replaced this one.
`Deprecated` points inward: the summary links to the section of this record
explaining the reversal, which is why withdrawing a decision needs no successor
record to stay accountable. A decision abandoned before anything was built on it
is usually `Deprecated`; one that shipped and was later replaced is
`Superseded`.

The current state is the last row, restated in the summary line so a reader who
never expands the block still knows where the record stands. The gap between
`Accepted` and `Implemented` is the number worth watching -- a decision the code
never honoured is a row that never arrived.

Only state transitions belong in the log. A change to what a record *says* is a
dated note in the body, beside the reasoning it concerns.

## Evidence has to be runnable

Code that produces concrete data used in the argumentation of an ADR has to be
committed somewhere a reader can run it -- a number quoted in an ADR has to be
re-derivable from a checkout, or the record is asserting rather than arguing.
An experiment needing BWAPI's own headers or sources goes in the sub-project at
[`docs/decisions/experiments/`](docs/decisions/experiments/) as an entry point
named after the record; one needing only the compiler and the standard library
goes in a [Compiler Explorer](https://godbolt.org) share link recorded in the
ADR.

**This repository inverts the usual preference between those two**, and the
reason is the same one that makes everything else here awkward: BWAPI is Win32
and MSVC, and an experiment compiled on a Linux box measures the Linux ABI. For
a question about layout, `sizeof`, packing or calling convention, Compiler
Explorer has real MSVC and is the *first* route, not the fallback.
[`docs/decisions/experiments/README.md`](docs/decisions/experiments/README.md#where-an-msvc-abi-question-actually-goes)
has the worked version, including a `sizeof` that is identical under both ABIs
and one that is not.

The playground is for recording a result, not for finding one. A share link is
minted on someone else's infrastructure, and it publishes a snippet that nobody
can collect afterwards. So an experiment is developed **locally** -- the
compiler against a file in a scratch directory, as many times as it takes -- and
goes to Compiler Explorer only once it produces the result the record is going
to quote. Nothing is minted to find out what happens; a link is minted to
publish what already happened.

Two consequences, stated outright because the natural working rhythm violates
both. **No iterative development against playground resources**: an experiment
that took four attempts should leave one link behind, not four, and reworking
after minting turns the earlier links into litter that stays live and wrong. And
**at most one link a minute** -- a record needing several is a record that should
mint them at that pace.

A playground experiment is written as four parts in a fixed order: a bolded
label saying what it demonstrates, the source in a code block, its output in a
`text` block, and a provenance line as a blockquote beneath them, reading
`TOOL VERSION (released DATE) - output checked DATE - [Compiler Explorer](URL)`.
The fences stay bare. Both dates matter: the released date belongs to the
compiler that produced the quoted output, the checked date is when someone last
confirmed the link still produces it.
[`docs/decisions/README.md`](docs/decisions/README.md#playground-experiments-carry-four-things-not-one)
has the skeleton.

**Reviewing a record includes checking two things.** First the version stamp: a
playground link re-runs against whatever its channel points at when it is
clicked, so any compiler output quoted in a record has to say which version
produced it, and a record carrying none cannot be checked later. Second the
status log, which needs its `Accepted` row before the record merges. Neither
should be approved on autopilot.

## Tests and research are not the same thing

An ADR arguing for a different internal design should not be accompanied by
tests. Such a test is written against the very structure the ADR exists to
replace, so landing the change means rewriting it -- it never guarded anything,
it just made the diff bigger. Support that argument with an experiment in
[`docs/decisions/experiments/`](docs/decisions/experiments/) instead, and let the
existing suite go on checking that behaviour did not change while the internals
did.

Research is evidence, not a test. It is not expected to keep passing, and it has
a scheduled exit:
[`docs/decisions/experiments/README.md`](docs/decisions/experiments/README.md#retirement)
says when and where it goes.

One exception, and it is the one BWAPI hits constantly. Where a record argues
that the implementation **diverges from something this project does not control**
-- Broodwar's actual behaviour, a struct layout a released client already
depends on, a documented API contract -- that is a bug report rather than a
design preference, and it does get a test. Write it against the external truth
so it stays correct after the fix, and mark it skipped with the record's number
and a reason, so the suite stays green while the reason stays visible.


# Versioning
BWAPI versioning is categorized as follows: `major.minor.patch [Beta]`

Version Component | Description
------------------|----------------
major             | Increased when massive structural changes are made.
minor             | Increased when breaking changes are made. Modules will need to be recompiled.
patch             | Increased when non-breaking changes are made.
Beta              | Appended to a major version increase until stability has been verified.

# Coding Standards

## Spacing
- Use double-spaces instead of tabs. You should be able to convert tabs to spaces automatically in your editor's settings.
- Always indent for each scope level.
- Always put a space between all arithmetic, binary, and conditional operators.
- Don't put a space between function names and the open parenthesis.
- Put a space after a comma.

Examples of spacing
~~~{.cpp}
// bad, can't tell if assignment is a typo, and
// there is a space between the member function and its parameters
if(Broodwar->canMake (UnitTypes::Terran_Marine, builder))
  reservedMinerals=-UnitTypes::Terran_Marine.mineralPrice();

// good, the arithmetic is clear
if(Broodwar->canMake(UnitTypes::Terran_Marine, builder))
  reservedMinerals = -UnitTypes::Terran_Marine.mineralPrice();

// bad, the scope is not indented, no space following the comma
if ( Broodwar->canMake(UnitTypes::Terran_Marine,builder) )
reservedMinerals = -UnitTypes::Terran_Marine.mineralPrice();

// good, alternative spacing style for if statements
if ( Broodwar->canMake(UnitTypes::Terran_Marine, builder) )
  reservedMinerals = -UnitTypes::Terran_Marine.mineralPrice();
~~~

## Formatting
- Use [ANSI style](http://en.wikipedia.org/wiki/Indent_style#Allman_style) braces.
- Break up large single-line if statements to be multi-line if statements.
- Format constructor initializer lists as follows (so that the colon is in the same column as the commas):

~~~{.cpp}
  BulletImpl::BulletImpl(BW::CBullet* originalBullet, u16 _index)
      : bwOriginalBullet(originalBullet)
      , index(_index)
  {
  }
~~~

## Naming
 - Use meaningful names. If a variable's purpose cannot be identified by another project member without analysing the code, then the variable needs to be renamed.
 - In local looping scopes, single-letter variable names are generally used as follows:
   - `b` for Bullet
   - `f` for Force
   - `p` for Player
   - `r` for Region
   - `u` for Unit
   - `i` for iterator/index
 - Member variables and member function names should be in [lower camel case](http://en.wikipedia.org/wiki/CamelCase). Examples: `getUnitsInRectangle`, `wasSeenByBWAPIPlayer`.
 - Constants and macros should be in ALL CAPS and words separated by underscores. Example: `PLAYER_COUNT`
 - Use [upper Camel Case](http://en.wikipedia.org/wiki/CamelCase) for classes, structures, enums, and namespaces.

## Hacking
 - Offsets must be in hexadecimal. Example: `0x00408CF0`.
 - Offsets must be placed in `BW/offsets.h`.
 - Avoid using inline assembly unless it is impossible to do so.
 - Always perform version checking before making code patches, to maintain partial cross-version compatibility.

## Documentation
 - Use the triple slash (`///`) format.
 - Try to include as much details on the function as possible. Redundancy can express clarity.
 - Include example code if possible.
 - Wrap lines before the 100th column.

### Doxygen Generation
 - Use `@` for doxygen commands.
 - Use `@see` to refer to other functions/classes.
 - When introducing a new function, include a `@since` tag with the version number.
 - Include `@returns`, and `@retval` where necessary.
 - When specifying `@param`, also specify if the argument is optional. Present an indented description on the next line, and also specify its default value if applicable.
  
### Intellisense Compatibility
 - Use the [summary tag](https://msdn.microsoft.com/en-us/library/ms177242.aspx) to wrap the function/class/enum description.
 - Use the [param tag](https://msdn.microsoft.com/en-us/library/ms177235.aspx) to identify all of the parameters.
 - Identify if an argument is optional with `(optional)` on the same line as the tag.
 - Put parameter descriptions indented on the next line, between the tags.


Examples of documentation:

~~~{.cpp}
    /// <summary>Sets the size of the text for all calls to drawText following this one.</summary>
    ///
    /// <param name="size"> (optional)
    ///   The size of the text. This value is one of Text::Size::Enum. If this value is omitted,
    ///   then a default value of Text::Size::Default is used.
    /// </param>
    ///
    /// Example usage
    /// @code
    ///   void ExampleAIModule::onFrame()
    ///   {
    ///     // Centers the name of the player in the upper middle of the screen
    ///     BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Large);
    ///     BWAPI::Broodwar->drawTextScreen(BWAPI::Positions::Origin, "%c%c%s",
    ///                                     BWAPI::Text::Align_Center,
    ///                                     BWAPI::Text::Green,
    ///                                     BWAPI::Broodwar->self()->getName().c_str() );
    ///     BWAPI::Broodwar->setTextSize();   // Set text size back to default
    ///   }
    /// @endcode
    /// @see Text::Size::Enum
    virtual void setTextSize(Text::Size::Enum size = Text::Size::Default) = 0;
~~~

~~~{.cpp}
    /// <summary>Retrieves the region at a given position.</summary>
    ///
    /// <param name="x">
    ///   The x coordinate, in pixels.
    /// </param>
    /// <param name="y">
    ///   The y coordinate, in pixels.
    /// </param>
    ///
    /// @returns Pointer to the Region interface at the given position.
    /// @retval nullptr if the provided position is not valid (i.e. not within the map bounds).
    ///
    /// @note If the provided position is invalid, the error Errors::Invalid_Parameter is set.
    /// @see getAllRegions, getRegion
    virtual BWAPI::Region getRegionAt(int x, int y) const = 0;
    /// @overload
    BWAPI::Region getRegionAt(BWAPI::Position position) const;
~~~


## Language Features
 - Use the `nullptr` keyword instead of `NULL`.
 - Create move constructors if appropriate.
 - Use the `const` keyword where appropriate.
 - Use std::array instead of C-style arrays.
 - Use explicit enum types (preferably enum class) instead of ints.
 - Use in-class member initialization.
 - Use the keywords default, delete, override, final, etc.

# Changing the API
## Adding a Virtual Function
  1. Add it after all other virtual functions.
  2. Label it with Doxygen `@since` tag, indicating the version it was introduced.
  
Note: Virtual functions are implementation defined, but Visual Studio appears to maintain some consistency regarding the use of virtual functions. Adding a new function to the end will maintain some backwards compatibility.
  
## Renaming a Function
  1. Rename the function.
  2. Create a non-virtual function with the old name that calls the new function.
  3. Label it with Doxygen `@deprecated` tag and refer to the new function.
  4. Optionally add a compiler deprecation warning (until we move to VS 2015).
  5. Remove it after the next 2 minor versions or next major version, whichever comes first.

## Deprecating a Function 
  1. Label it with Doxygen `@deprecated` tag.
  2. Provide reason for the deprecation and alternatives if applicable.
  3. Optionally add a compiler deprecation warning (until we move to VS 2015).
  4. Remove it after the next 2 minor versions or next major version, whichever comes first.

