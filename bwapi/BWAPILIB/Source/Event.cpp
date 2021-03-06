#include <BWAPI/Event.h>

#include "../../Debug.h"

namespace BWAPI
{
  std::string emptyString;
  Event::Event()
    :type(EventType::None)
    ,position(Positions::None)
    ,text(NULL)
    ,unit(NULL)
    ,player(NULL)
    ,winner(false)
  {
  }
  Event::Event(const Event& other)
  {
    type = other.type;
    position = other.position;
    if (other.text!=NULL)
      text = new std::string(*other.text);
    else
      text = NULL;
    unit = other.unit;
    player = other.player;
    winner = other.winner;
  }
  Event::~Event()
  {
    if (text!=NULL)
      delete text;
    text=NULL;
  }
  Event& Event::operator=(const Event& other)
  {
    type = other.type;
    position = other.position;
    if (other.text!=NULL)
      text = new std::string(*other.text);
    else
      text = NULL;
    unit = other.unit;
    player = other.player;
    winner = other.winner;
    return *this;
  }
  bool Event::operator==(const Event& other) const
  {
    return (type     == other.type &&
            position == other.position &&
            ((text==NULL && other.text==NULL) || (text!=NULL && other.text!=NULL &&*text == *other.text)) &&
            unit     == other.unit &&
            player   == other.player &&
            winner == other.winner);
  }
  Event Event::MatchStart()
  {
    Event e;
    e.type = EventType::MatchStart;
    return e;
  }
  Event Event::MatchEnd(bool isWinner)
  {
    Event e;
    e.type   = EventType::MatchEnd;
    e.winner = isWinner;
    return e;
  }
  Event Event::MatchFrame()
  {
    Event e;
    e.type = EventType::MatchFrame;
    return e;
  }
  Event Event::MenuFrame()
  {
    Event e;
    e.type = EventType::MenuFrame;
    return e;
  }
  Event Event::SendText(const char* text)
  {
    Event e;
    e.type = EventType::SendText;
    if (text!=NULL)
      e.text = new std::string(text);
    return e;
  }
  Event Event::ReceiveText(Player* player, const char* text)
  {
    Event e;
    e.type   = EventType::ReceiveText;
    e.player = player;
    if (text!=NULL)
      e.text   = new std::string(text);
    return e;
  }
  Event Event::PlayerLeft(Player* player)
  {
    Event e;
    e.type   = EventType::PlayerLeft;
    e.player = player;
    return e;
  }
  Event Event::PlayerDropped(Player* player)
  {
    Event e;
    e.type   = EventType::PlayerDropped;
    e.player = player;
    return e;
  }
  Event Event::NukeDetect(Position target)
  {
    Event e;
    e.type     = EventType::NukeDetect;
    e.position = target;
    return e;
  }
  Event Event::UnitDiscover(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitDiscover;
    e.unit = unit;
    return e;
  }
  Event Event::UnitEvade(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitEvade;
    e.unit = unit;
    return e;
  }
  Event Event::UnitShow(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitShow;
    e.unit = unit;
    return e;
  }
  Event Event::UnitHide(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitHide;
    e.unit = unit;
    return e;
  }
  Event Event::UnitCreate(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitCreate;
    e.unit = unit;
    return e;
  }
  Event Event::UnitDestroy(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitDestroy;
    e.unit = unit;
    return e;
  }
  Event Event::UnitMorph(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitMorph;
    e.unit = unit;
    return e;
  }
  Event Event::UnitRenegade(Unit* unit)
  {
    Event e;
    e.type = EventType::UnitRenegade;
    e.unit = unit;
    return e;
  }
  Event Event::SaveGame(const char* gameName)
  {
    Event e;
    e.type = EventType::SaveGame;
    if (gameName!=NULL)
      e.text = new std::string(gameName);
    return e;
  }
  Event Event::UnitComplete(Unit *unit)
  {
    Event e;
    e.type = EventType::UnitComplete;
    e.unit = unit;
    return e;
  }
  EventType::Enum Event::getType() const
  {
    return type;
  }
  Position Event::getPosition() const
  {
    return position;
  }
  const std::string& Event::getText() const
  {
    if (text==NULL)
      return emptyString;
    return *text;
  }
  Unit* Event::getUnit() const
  {
    return unit;
  }
  Player* Event::getPlayer() const
  {
    return player;
  }
  bool Event::isWinner() const
  {
    return winner;
  }
  Event& Event::setType(EventType::Enum type)
  {
    this->type = type;
    return *this;
  }
  Event& Event::setPosition(Position position)
  {
    this->position = position;
    return *this;
  }
  Event& Event::setText(const char* text)
  {
    if (this->text!=NULL)
    {
      if (text!=NULL)
      {
        this->text->assign(text);
      }
      else
      {
        delete this->text;
        this->text = NULL;
      }
    }
    else
    {
      if (text!=NULL)
      {
        this->text = new std::string(text);
      }
    }
    return *this;
  }
  Event& Event::setUnit(Unit* unit)
  {
    this->unit = unit;
    return *this;
  }
  Event& Event::setPlayer(Player* player)
  {
    this->player = player;
    return *this;
  }
  Event& Event::setWinner(bool isWinner)
  {
    this->winner = isWinner;
    return *this;
  }
}