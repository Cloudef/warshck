#include "game.h"
#include <iostream>
#include <sstream>
#include <algorithm>

#include "jsonpp.h"

std::unordered_map<std::string, wars::Game::State> const wars::Game::STATE_NAMES = {
  {"pregame", State::PREGAME},
  {"inProgress", State::IN_PROGRESS},
  {"finished", State::FINISHED}
};

namespace
{
  template<typename T>
  T parse(json::Value const& v);

  template<typename T>
  std::unordered_map<int, T> parseAll(json::Value const& v);

  template<>
  wars::Weapon parse(json::Value const& v);
  template<>
  wars::Armor parse(json::Value const& v);
  template<>
  wars::UnitClass parse(json::Value const& v);
  template<>
  wars::TerrainFlag parse(json::Value const& v);
  template<>
  wars::TerrainType parse(json::Value const& v);
  template<>
  wars::MovementType parse(json::Value const& v);
  template<>
  wars::UnitFlag parse(json::Value const& v);
  template<>
  wars::UnitType parse(json::Value const& v);
  template<>
  wars::Rules parse(json::Value const& value);

  std::unordered_map<int, int> parseIntIntMap(json::Value const& v);
  std::unordered_map<int, int> parseIntIntMapWithNulls(json::Value const& v, int nullValue);
  std::unordered_set<int> parseIntSet(json::Value const& v);
  int parseIntOrNull(json::Value const& v, int nullValue);
  std::string parseStringOrNull(json::Value const& v, std::string const& nullValue);
  wars::Game::Path parsePath(json::Value const& v);
}
wars::Game::Game(): gameId(), authorId(),  name(), mapId(),
  state(State::PREGAME), turnStart(0), turnNumber(0), roundNumber(0), inTurnNumber(0),
  publicGame(false), turnLength(0), bannedUnits(0),
  rules(), tiles(), units(),  players(), eventStream()
{

}

wars::Game::~Game()
{

}

Stream<wars::Game::Event> wars::Game::events()
{
  return eventStream;
}

void wars::Game::setRulesFromJSON(const json::Value& value)
{
  rules = parse<Rules>(value);
}

void wars::Game::setGameDataFromJSON(const json::Value& value)
{
  json::Value game = value.get("game");
  gameId = game.get("gameId").stringValue();
  authorId = game.get("authorId").stringValue();
  name = game.get("name").stringValue();
  mapId = game.get("mapId").stringValue();
  state = STATE_NAMES.at(game.get("state").stringValue());
  turnStart = game.get("turnStart").longValue();
  turnNumber = game.get("turnNumber").longValue();
  roundNumber = game.get("roundNumber").longValue();
  inTurnNumber = game.get("inTurnNumber").longValue();

  json::Value settings = game.get("settings");
  publicGame = settings.get("public").booleanValue();
  turnLength = parseIntOrNull(settings.get("turnLength"), -1);
  bannedUnits = parseIntSet(settings.get("bannedUnits"));

  json::Value tileArray = game.get("tiles");
  unsigned int numTiles = tileArray.size();
  for(unsigned int i = 0; i < numTiles; ++i)
  {
    json::Value tile = tileArray.at(i);
    updateTileFromJSON(tile);
  }

  json::Value playerArray = game.get("players");
  unsigned int numPlayers = playerArray.size();
  for(unsigned int i = 0; i < numPlayers; ++i)
  {
    json::Value player = playerArray.at(i);
    updatePlayerFromJSON(player);
  }

  Event event;
  event.type = EventType::GAMEDATA;
  eventStream.push(event);
}

void wars::Game::processEventFromJSON(const json::Value& value)
{
  json::Value content = value.get("content");
  std::string action = content.get("action").stringValue();

  if(action == "move")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    std::string tileId = content.get("tile").get("tileId").stringValue();
    Path path = parsePath(content.get("path"));
    moveUnit(unitId, tileId, path);
  }
  else if(action == "wait")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    waitUnit(unitId);
  }
  else if(action == "attack")
  {
    std::string attackerId = content.get("attacker").get("unitId").stringValue();
    std::string targetId = content.get("target").get("unitId").stringValue();
    int damage = content.get("damage").longValue();
    attackUnit(attackerId, targetId, damage);
  }
  else if(action == "counterattack")
  {
    std::string attackerId = content.get("attacker").get("unitId").stringValue();
    std::string targetId = content.get("target").get("unitId").stringValue();
    int damage = -1;
    if(content.get("damage").type() == json::Value::Type::NUMBER)
    {
      damage = content.get("damage").longValue();
    }
    counterattackUnit(attackerId, targetId, damage);
  }
  else if(action == "capture")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    std::string tileId = content.get("tile").get("tileId").stringValue();
    int left = content.get("left").longValue();
    captureTile(unitId, tileId, left);
  }
  else if(action == "captured")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    std::string tileId = content.get("tile").get("tileId").stringValue();
    capturedTile(unitId, tileId);
  }
  else if(action == "deploy")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    deployUnit(unitId);
  }
  else if(action == "undeploy")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    undeployUnit(unitId);
  }
  else if(action == "load")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    std::string carrierId = content.get("carrier").get("unitId").stringValue();
    loadUnit(unitId, carrierId);
  }
  else if(action == "unload")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    std::string carrierId = content.get("carrier").get("unitId").stringValue();
    std::string tileId = content.get("tile").get("tileId").stringValue();
    unloadUnit(unitId, carrierId, tileId);
  }
  else if(action == "destroyed")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    destroyUnit(unitId);
  }
  else if(action == "repair")
  {
    std::string unitId = content.get("unit").get("unitId").stringValue();
    int newHealth = content.get("newHealth").longValue();
    repairUnit(unitId, newHealth);
  }
  else if(action == "build")
  {
    std::string tileId = content.get("tile").get("tileId").stringValue();
    std::string unitId = updateUnitFromJSON(content.get("unit"));
    units[unitId].tileId = tileId;
    buildUnit(tileId, unitId);
  }
  else if(action == "regenerateCapturePoints")
  {
    std::string tileId = content.get("tile").get("tileId").stringValue();
    int newCapturePoints = content.get("newCapturePoints").longValue();
    regenerateCapturePointsTile(tileId, newCapturePoints);
  }
  else if(action == "produceFunds")
  {
    std::string tileId = content.get("tile").get("tileId").stringValue();
    produceFundsTile(tileId);
  }
  else if(action == "beginTurn")
  {
    int playerNumber = content.get("player").longValue();
    beginTurn(playerNumber);
  }
  else if(action == "endTurn")
  {
    int playerNumber = content.get("player").longValue();
    endTurn(playerNumber);
  }
  else if(action == "turnTimeout")
  {
    int playerNumber = content.get("player").longValue();
    turnTimeout(playerNumber);
  }
  else if(action == "finished")
  {
    int winnerPlayerNumber = content.get("winner").longValue();
    finished(winnerPlayerNumber);
  }
  else if(action == "surrender")
  {
    int playerNumber = content.get("player").longValue();
    surrender(playerNumber);
  }
  else
  {
    std::cerr << "Unknown event action: " << action << std::endl;
  }
}

void wars::Game::processEventsFromJSON(const json::Value& value)
{
  unsigned int numEvents = value.size();
  for(int i = 0; i < numEvents; ++i)
  {
    json::Value event = value.at(i);
    processEventFromJSON(event);
  }
}

void wars::Game::moveUnit(std::string const& unitId, std::string const& tileId, Path const& path)
{
  Event event;
  event.type = EventType::MOVE;
  event.move.unitId = &unitId;
  event.move.tileId = &tileId;
  event.move.path = &path;
  eventStream.push(event);

  Unit& unit = units.at(unitId);
  Tile& tile = tiles.at(tileId);
  tiles.at(unit.tileId).unitId.clear();
  if(tile.unitId.empty())
    tile.unitId = unitId;
  unit.tileId = tileId;
}

void wars::Game::waitUnit(std::string const& unitId)
{
  Event event;
  event.type = EventType::WAIT;
  event.wait.unitId = &unitId;
  eventStream.push(event);

  units.at(unitId).moved = true;
}

void wars::Game::attackUnit(std::string const& attackerId, std::string const& targetId, int damage)
{
  Event event;
  event.type = EventType::ATTACK;
  event.attack.attackerId = &attackerId;
  event.attack.targetId = &targetId;
  event.attack.damage = damage;
  eventStream.push(event);

units.at(attackerId).moved = true;
units.at(targetId).health -= damage;
}

void wars::Game::counterattackUnit(std::string const& attackerId, std::string const& targetId, int damage)
{
  Event event;
  event.type = EventType::COUNTERATTACK;
  event.counterattack.attackerId = &attackerId;
  event.counterattack.targetId = &targetId;
  event.counterattack.damage = damage;
  eventStream.push(event);

  units.at(targetId).health -= damage;
}

void wars::Game::captureTile(std::string const& unitId, std::string const& tileId, int left)
{
  Event event;
  event.type = EventType::CAPTURE;
  event.capture.unitId = &unitId;
  event.capture.tileId = &tileId;
  event.capture.left = left;
  eventStream.push(event);

  units.at(unitId).moved = true;
  Tile& tile = tiles.at(tileId);
  tile.capturePoints = left;
  tile.beingCaptured = true;
}

void wars::Game::capturedTile(std::string const& unitId, std::string const& tileId)
{
  Event event;
  event.type = EventType::CAPTURED;
  event.captured.unitId = &unitId;
  event.captured.tileId = &tileId;
  eventStream.push(event);

  Tile& tile = tiles.at(tileId);
  tile.capturePoints = 1;
  tile.beingCaptured = false;
  tile.owner = units.at(unitId).owner;
}

void wars::Game::deployUnit(std::string const& unitId)
{
  Event event;
  event.type = EventType::DEPLOY;
  event.deploy.unitId = &unitId;
  eventStream.push(event);

  Unit& unit = units.at(unitId);
  unit.moved = true;
  unit.deployed = true;
}

void wars::Game::undeployUnit(std::string const& unitId)
{
  Event event;
  event.type = EventType::UNDEPLOY;
  event.undeploy.unitId = &unitId;
  eventStream.push(event);

  Unit& unit = units.at(unitId);
  unit.moved = true;
  unit.deployed = false;
}

void wars::Game::loadUnit(std::string const& unitId, std::string const& carrierId)
{
  Event event;
  event.type = EventType::LOAD;
  event.load.unitId = &unitId;
  event.load.carrierId = &carrierId;
  eventStream.push(event);

  Unit& unit = units.at(unitId);
  unit.tileId.clear();
  unit.carriedBy = carrierId;
  unit.moved = true;
  Unit& carrier = units.at(carrierId);
  carrier.carriedUnits.push_back(unitId);
}

void wars::Game::unloadUnit(std::string const& unitId, std::string const& carrierId, std::string const& tileId)
{
  Event event;
  event.type = EventType::UNLOAD;
  event.unload.unitId = &unitId;
  event.unload.carrierId = &carrierId;
  event.unload.tileId = &tileId;
  eventStream.push(event);

  Unit& unit = units.at(unitId);
  unit.tileId = tileId;
  unit.moved = true;
  tiles.at(tileId).unitId = unitId;
  Unit& carrier = units[carrierId];
  carrier.moved = true;
  std::remove(carrier.carriedUnits.begin(), carrier.carriedUnits.end(), unitId);
}

void wars::Game::destroyUnit(std::string const& unitId)
{
  Event event;
  event.type = EventType::DESTROY;
  event.destroy.unitId = &unitId;
  eventStream.push(event);

  Unit unit = units.at(unitId);
  if(!unit.tileId.empty())
    tiles.at(unit.tileId).unitId.erase();

  for(std::string const& carriedUnitId : unit.carriedUnits)
  {
    destroyUnit(carriedUnitId);
  }

  units.erase(unitId);
}

void wars::Game::repairUnit(std::string const& unitId, int newHealth)
{
  Event event;
  event.type = EventType::REPAIR;
  event.repair.unitId = &unitId;
  event.repair.newHealth = newHealth;
  eventStream.push(event);

  units.at(unitId).health = newHealth;
}

void wars::Game::buildUnit(std::string const& tileId, std::string const& unitId)
{
  Event event;
  event.type = EventType::BUILD;
  event.build.tileId = &tileId;
  event.build.unitId = &unitId;
  eventStream.push(event);

  tiles.at(tileId).unitId = unitId;
  units.at(unitId).moved = true;
}

void wars::Game::regenerateCapturePointsTile(std::string const& tileId, int newCapturePoints)
{
  Event event;
  event.type = EventType::REGENERATE_CAPTURE_POINTS;
  event.regenerateCapturePoints.tileId = &tileId;
  event.regenerateCapturePoints.newCapturePoints = newCapturePoints;
  eventStream.push(event);

  Tile& tile = tiles.at(tileId);
  tile.capturePoints = newCapturePoints;
  tile.beingCaptured = false;

}

void wars::Game::produceFundsTile(std::string const& tileId)
{
  Event event;
  event.type = EventType::PRODUCE_FUNDS;
  event.produceFunds.tileId = &tileId;
  eventStream.push(event);
}

void wars::Game::beginTurn(int playerNumber)
{
  Event event;
  event.type = EventType::BEGIN_TURN;
  event.beginTurn.playerNumber = playerNumber;
  eventStream.push(event);

  inTurnNumber = playerNumber;
}

void wars::Game::endTurn(int playerNumber)
{
  Event event;
  event.type = EventType::END_TURN;
  event.endTurn.playerNumber = playerNumber;
  eventStream.push(event);

  for(auto& item : units)
  {
    Unit& unit = item.second;
    unit.moved = false;
  }
}

void wars::Game::turnTimeout(int playerNumber)
{
  Event event;
  event.type = EventType::TURN_TIMEOUT;
  event.turnTimeout.playerNumber = playerNumber;
  eventStream.push(event);
}

void wars::Game::finished(int winnerPlayerNumber)
{
  Event event;
  event.type = EventType::FINISHED;
  event.finished.winnerPlayerNumber = winnerPlayerNumber;
  eventStream.push(event);

  state = State::FINISHED;
}

void wars::Game::surrender(int playerNumber)
{
  Event event;
  event.type = EventType::SURRENDER;
  event.surrender.playerNumber = playerNumber;
  eventStream.push(event);

  std::vector<std::string> unitsToDestroy;
  for(auto& item : units)
  {
    Unit& unit = item.second;
    if(unit.owner == playerNumber)
    {
      unitsToDestroy.push_back(unit.id);
    }
  }

  for(std::string const& unitId : unitsToDestroy)
  {
    destroyUnit(unitId);
  }

  for(auto& item : tiles)
  {
    Tile& tile = item.second;
    if(tile.owner == playerNumber)
    {
      tile.owner = NEUTRAL_PLAYER_NUMBER;
    }
  }
}

wars::Game::Tile const & wars::Game::getTile(const std::string& tileId) const
{
  return tiles.at(tileId);
}

wars::Game::Unit const& wars::Game::getUnit(const std::string& unitId) const
{
  return units.at(unitId);
}

wars::Game::Player const& wars::Game::getPlayer(int playerNumber) const
{
  return players.at(playerNumber);
}

const std::unordered_map<std::string, wars::Game::Tile>& wars::Game::getTiles() const
{
  return tiles;
}

const std::unordered_map<std::string, wars::Game::Unit>& wars::Game::getUnits() const
{
  return units;
}

const std::unordered_map<int, wars::Game::Player>& wars::Game::getPlayers() const
{
  return players;
}

const wars::Rules& wars::Game::getRules() const
{
  return rules;
}

wars::Game::Player const& wars::Game::getInTurn()
{
  return players.at(inTurnNumber) ;
}

const wars::Game::Tile* wars::Game::getTileAt(int x, int y) const
{
  for(auto const& item : tiles)
  {
    if(item.second.x == x && item.second.y == y)
    {
      return &item.second;
    }
  }

  return nullptr;
}

const std::string& wars::Game::getGameId() const
{
  return gameId;
}

std::string wars::Game::updateTileFromJSON(const json::Value& value)
{
  Tile tile;
  tile.id = value.get("tileId").stringValue();
  tile.x = value.get("x").longValue();
  tile.y = value.get("y").longValue();
  tile.type = value.get("type").longValue();
  tile.subtype = value.get("subtype").longValue();
  tile.owner = value.get("owner").longValue();
  tile.capturePoints = value.get("capturePoints").longValue();
  tile.beingCaptured = value.get("beingCaptured").booleanValue();
  tile.unitId = parseStringOrNull(value.get("unitId"), "");

  if(!tile.unitId.empty())
  {
    updateUnitFromJSON(value.get("unit"));
  }

  tiles[tile.id] = tile;
  return tile.id;
}

std::string wars::Game::updateUnitFromJSON(const json::Value& value)
{
  std::string unitId = value.get("unitId").stringValue();

  auto iter = units.find(unitId);
  if(iter == units.end())
  {
    Unit u;
    u.id = unitId;
    units[unitId] = u;
  }
  Unit& unit =  units[unitId];

  if(value.has("owner"))
    unit.owner = value.get("owner").longValue();
  if(value.has("type"))
    unit.type = value.get("type").longValue();
  if(value.has("tileId"))
    unit.tileId = parseStringOrNull(value.get("tileId"), "");
  if(value.has("carriedBy"))
    unit.carriedBy = parseStringOrNull(value.get("carriedBy"), "");
  if(value.has("health"))
    unit.health = value.get("health").longValue();
  if(value.has("deployed"))
    unit.deployed = value.get("deployed").booleanValue();
  if(value.has("moved"))
    unit.moved = value.get("moved").booleanValue();
  if(value.has("capturing"))
    unit.capturing = value.get("capturing").booleanValue();

  if(value.has("carriedUnits"))
  {
    json::Value carriedUnits = value.get("carriedUnits");
    unsigned int numCarriedUnits = carriedUnits.size();
    for(unsigned int i = 0; i < numCarriedUnits; ++i)
    {
      json::Value carriedUnit = carriedUnits.at(i);
      std::string carriedUnitId = updateUnitFromJSON(carriedUnit);
      unit.carriedUnits.push_back(carriedUnitId);
    }
  }
  return unit.id;
}

int wars::Game::updatePlayerFromJSON(const json::Value& value)
{
  int playerNumber = value.get("playerNumber").longValue();

  auto iter = players.find(playerNumber);
  if(iter == players.end())
  {
    Player p;
    p.playerNumber = playerNumber;
    players[playerNumber] = p;
  }
  Player& player =  players[playerNumber];

  if(value.has("_id"))
    player.id = value.get("_id").stringValue();
  if(value.has("userId"))
    player.userId = parseStringOrNull(value.get("userId"), "");
  if(value.has("playerName"))
    player.playerName = parseStringOrNull(value.get("playerName"), "");
  if(value.has("teamNumber"))
    player.teamNumber = value.get("teamNumber").longValue();
  if(value.get("funds").type() == json::Value::Type::NUMBER)
    player.funds = value.get("funds").longValue();
  if(value.has("score"))
    player.score = value.get("score").longValue();
  if(value.has("isMe"))
    player.isMe = value.get("isMe").booleanValue();
  if(value.get("settings").type() == json::Value::Type::OBJECT)
  {
    json::Value settings = value.get("settings");
    if(settings.has("emailNotifications"))
      player.emailNotifications = settings.get("emailNotifications").booleanValue();
    if(settings.has("hidden"))
      player.hidden = settings.get("hidden").booleanValue();
  }

  return playerNumber;
}

namespace
{
  template<typename T>
  std::unordered_map<int, T> parseAll(json::Value const& v)
  {
    std::unordered_map<int, T> result;
    std::vector<std::string> ids = v.properties();
    for(std::string id : ids)
    {
      json::Value json = v.get(id.data());
      T t = parse<T>(json);
      result[t.id] = t;
    }
    return result;
  }

  template<>
  wars::Weapon parse(json::Value const& v)
  {
    wars::Weapon weapon;
    weapon.id = v.get("id").longValue();
    weapon.name = v.get("name").stringValue();
    weapon.requireDeployed = v.get("requireDeployed").booleanValue();

    weapon.powerMap = parseIntIntMap(v.get("powerMap"));
    weapon.rangeMap = parseIntIntMap(v.get("rangeMap"));

    return weapon;
  }

  std::unordered_map<int, int> parseIntIntMap(json::Value const& v)
  {
    std::vector<std::string> props = v.properties();
    std::unordered_map<int, int> result;
    for(std::string const& s : props)
    {
      int i = 0;
      std::istringstream(s) >> i;
      result[i] = v.get(s).longValue();
    }

    return result;
  }

  std::unordered_map<int, int> parseIntIntMapWithNulls(json::Value const& v, int nullValue)
  {
    std::vector<std::string> props = v.properties();
    std::unordered_map<int, int> result;
    for(std::string const& s : props)
    {
      int i = 0;
      std::istringstream(s) >> i;
      json::Value prop = v.get(s);
      if(prop.type() == json::Value::Type::NULL_JSON)
      {
        result[i] = nullValue;
      }
      else
      {
        result[i] = prop.longValue();
      }
    }

    return result;
  }

  template<>
  wars::Armor parse(json::Value const& v)
  {
    wars::Armor armor;
    armor.id = v.get("id").longValue();
    armor.name = v.get("name").stringValue();
    return armor;
  }
  template<>
  wars::UnitClass parse(json::Value const& v)
  {
    wars::UnitClass value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    return value;
  }

  template<>
  wars::TerrainFlag parse(json::Value const& v)
  {
    wars::TerrainFlag value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    return value;
  }

  template<>
  wars::TerrainType parse(json::Value const& v)
  {
    wars::TerrainType value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    value.buildTypes = parseIntSet(v.get("buildTypes"));
    value.repairTypes = parseIntSet(v.get("repairTypes"));
    value.flags = parseIntSet(v.get("flags"));
    return value;
  }

  template<>
  wars::MovementType parse(json::Value const& v)
  {
    wars::MovementType value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    value.effectMap = parseIntIntMapWithNulls(v.get("effectMap"), -1);
    return value;
  }

  template<>
  wars::UnitFlag parse(json::Value const& v)
  {
    wars::UnitFlag value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    return value;
  }

  template<>
  wars::UnitType parse(json::Value const& v)
  {
    wars::UnitType value;
    value.id = v.get("id").longValue();
    value.name = v.get("name").stringValue();
    value.unitClass = v.get("unitClass").longValue();
    value.price = v.get("price").longValue();
    value.primaryWeapon = parseIntOrNull(v.get("primaryWeapon"), -1);
    value.secondaryWeapon = parseIntOrNull(v.get("secondaryWeapon"), -1);
    value.armor = v.get("armor").longValue();
    value.defenseMap = parseIntIntMap(v.get("defenseMap"));
    value.movementType = v.get("movementType").longValue();
    value.movement = v.get("movement").longValue();
    value.carryClasses = parseIntSet(v.get("carryClasses"));
    value.carryNum = v.get("carryNum").longValue();
    value.flags = parseIntSet(v.get("flags"));

    return value;
  }

  template<>
  wars::Rules parse(json::Value const& value)
  {
    wars::Rules rules;
    rules.weapons = parseAll<wars::Weapon>(value.get("weapons"));
    rules.armors = parseAll<wars::Armor>(value.get("armors"));
    rules.unitClasses = parseAll<wars::UnitClass>(value.get("unitClasses"));
    rules.terrainFlags = parseAll<wars::TerrainFlag>(value.get("terrainFlags"));
    rules.terrainTypes = parseAll<wars::TerrainType>(value.get("terrains"));
    rules.movementTypes = parseAll<wars::MovementType>(value.get("movementTypes"));
    rules.unitFlags = parseAll<wars::UnitFlag>(value.get("unitFlags"));
    rules.unitTypes = parseAll<wars::UnitType>(value.get("units"));
    return rules;
  }

  std::unordered_set<int> parseIntSet(json::Value const& v)
  {
    std::unordered_set<int> result;
    for(unsigned int i = 0; i < v.size(); ++i)
    {
      result.insert(v.at(i).longValue());
    }
    return result;
  }
  int parseIntOrNull(json::Value const& v, int nullValue)
  {
    if(v.type() == json::Value::Type::NULL_JSON)
    {
      return nullValue;
    }
    else
    {
      return v.longValue();
    }
  }
  std::string parseStringOrNull(json::Value const& v, std::string const& nullValue)
  {
    if(v.type() == json::Value::Type::NULL_JSON)
    {
      return nullValue;
    }
    else
    {
      return v.stringValue();
    }

  }
  wars::Game::Path parsePath(json::Value const& v)
  {
    wars::Game::Path path;
    unsigned int numCoordinates = v.size();
    for(int i = 0; i < numCoordinates; ++i)
    {
      json::Value value = v.at(i);
      int x = value.get("x").longValue();
      int y = value.get("y").longValue();
      path.push_back({x, y});
    }
    return path;
  }
}
