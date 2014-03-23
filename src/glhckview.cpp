#include "glhckview.h"
#include <iostream>
#include <stdexcept>

namespace
{
  void errorCallback(int code, char const* message)
  {
    std::cerr << "GLFW ERROR: " << message << std::endl;
  }

  void windowCloseCallback(GLFWwindow* window)
  {
    wars::GlhckView* view = static_cast<wars::GlhckView*>(glfwGetWindowUserPointer(window));
    view->quit();
  }

  void windowSizeCallback(GLFWwindow *window, int width, int height)
  {
    glhckDisplayResize(width, height);
  }
}


kmVec3 const wars::GlhckView::_xBase = {1.5, -0.5, 0};
kmVec3 const wars::GlhckView::_yBase = {0, -1, 0};
kmVec3 const wars::GlhckView::_zBase = {0, 0, 1};

void wars::GlhckView::init(int argc, char** argv)
{
  if (!glfwInit())
  {
    throw std::runtime_error("GLFW initialization error");
  }
  glfwSetErrorCallback(errorCallback);

  glhckCompileFeatures features;
  glhckGetCompileFeatures(&features);
  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_SAMPLES, 4);
  if(features.render.glesv1 || features.render.glesv2) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
  }
  if(features.render.glesv2) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  }
  if(features.render.opengl) {
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
  }

  if(!glhckContextCreate(argc, argv))
  {
    throw std::runtime_error("Failed to create a GLhck context");
  }
}

void wars::GlhckView::term()
{
  glhckContextTerminate();
  glfwTerminate();
}

wars::GlhckView::GlhckView() : _window(nullptr), _shouldQuit(false), _units(), _tiles()
{
  _window = glfwCreateWindow(800, 480, "warshck", NULL, NULL);

  if(!_window)
  {
    throw std::runtime_error("Failed to create a GLFW window");
  }

  glfwSetWindowUserPointer(_window, this);

  glfwMakeContextCurrent(_window);

  glfwSetWindowCloseCallback(_window, windowCloseCallback);
  glfwSetWindowSizeCallback(_window, windowSizeCallback);

  glfwSwapInterval(1);


  glhckLogColor(0);
  if(!glhckDisplayCreate(800, 480, GLHCK_RENDER_AUTO))
  {
    throw std::runtime_error("Failed to create a GLhck display");
  }

  glhckRenderClearColorb(64, 64, 64, 255);

  _camera = glhckCameraNew();
  glhckCameraProjection(_camera, GLHCK_PROJECTION_PERSPECTIVE);
  glhckObjectPositionf(glhckCameraGetObject(_camera), 5, -25, 15);
  glhckObjectRotatef(glhckCameraGetObject(_camera), 0, 0,-90);
  glhckObjectTargetf(glhckCameraGetObject(_camera),10, 0, 0);

  glhckCameraRange(_camera, 0.1f, 100.0f);
  glhckCameraFov(_camera, 45);
  glhckCameraUpdate(_camera);
}

wars::GlhckView::~GlhckView()
{
}

void wars::GlhckView::setGame(Game* game)
{
  _game = game;

  _game->events().on<void>([this](wars::Game::Event const& e) {
    switch(e.type)
    {
      case wars::Game::EventType::GAMEDATA:
      {
        initializeFromGame();
        break;
      }
      case wars::Game::EventType::MOVE:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.move.unitId);
        wars::Game::Tile const& next = _game->getTile(*e.move.tileId);
        wars::Game::Tile const& prev = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::WAIT:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.wait.unitId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::ATTACK:
      {
        wars::Game::Unit const& attacker = _game->getUnit(*e.attack.attackerId);
        wars::Game::Unit const& target = _game->getUnit(*e.attack.targetId);
        break;
      }
      case wars::Game::EventType::COUNTERATTACK:
      {
        wars::Game::Unit const& attacker = _game->getUnit(*e.counterattack.attackerId);
        wars::Game::Unit const& target = _game->getUnit(*e.counterattack.targetId);
        break;
      }
      case wars::Game::EventType::CAPTURE:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.capture.unitId);
        wars::Game::Tile const& tile = _game->getTile(*e.capture.tileId);
        break;
      }
      case wars::Game::EventType::CAPTURED:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.captured.unitId);
        wars::Game::Tile const& tile = _game->getTile(*e.captured.tileId);
        break;
      }
      case wars::Game::EventType::DEPLOY:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.deploy.unitId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::UNDEPLOY:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.undeploy.unitId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::LOAD:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.load.unitId);
        wars::Game::Unit const& carrier = _game->getUnit(*e.load.carrierId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::UNLOAD:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.unload.unitId);
        wars::Game::Unit const& carrier = _game->getUnit(*e.unload.carrierId);
        wars::Game::Tile const& next = _game->getTile(*e.unload.tileId);
        break;
      }
      case wars::Game::EventType::DESTROY:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.destroy.unitId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::REPAIR:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.repair.unitId);
        wars::Game::Tile const& curr = _game->getTile(unit.tileId);
        break;
      }
      case wars::Game::EventType::BUILD:
      {
        wars::Game::Unit const& unit = _game->getUnit(*e.build.unitId);
        wars::Game::Tile const& tile = _game->getTile(*e.build.tileId);
        break;
      }
      case wars::Game::EventType::REGENERATE_CAPTURE_POINTS:
      {
        wars::Game::Tile const& tile = _game->getTile(*e.regenerateCapturePoints.tileId);
        break;
      }
      case wars::Game::EventType::PRODUCE_FUNDS:
      {
        wars::Game::Tile const& tile = _game->getTile(*e.produceFunds.tileId);
        break;
      }
      case wars::Game::EventType::BEGIN_TURN:
      {
        break;
      }
      case wars::Game::EventType::END_TURN:
      {
        break;
      }
      case wars::Game::EventType::TURN_TIMEOUT:
      {
        break;
      }
      case wars::Game::EventType::FINISHED:
      {
        break;
      }
      case wars::Game::EventType::SURRENDER:
      {
        break;
      }
      default:
      {
        break;
      }
    }
  });
}

bool wars::GlhckView::handle()
{
  glfwPollEvents();

  glhckCameraUpdate(_camera);

  for(auto& item : _units)
  {
    glhckObjectDraw(item.second.obj);
  }

  for(auto& item : _tiles)
  {
    glhckObjectDraw(item.second.obj);
  }

  glhckRenderClear(GLHCK_DEPTH_BUFFER_BIT | GLHCK_COLOR_BUFFER_BIT);
  glhckRender();
  glfwSwapBuffers(_window);

  return !_shouldQuit;
}

void wars::GlhckView::quit()
{
  _shouldQuit = true;
}

kmVec3 wars::GlhckView::hexToRect(const kmVec3& v)
{
  kmVec3 result, r;
  kmVec3Scale(&result, &_xBase, v.x);
  kmVec3Scale(&r, &_yBase, v.y);
  kmVec3Add(&result, &result, &r);
  kmVec3Scale(&r, &_zBase, v.z);
  kmVec3Add(&result, &result, &r);
  return result;
}

void wars::GlhckView::clear()
{
  for(auto& item : _units)
  {
    glhckObjectFree(item.second.obj);
  }
  _units.clear();

  for(auto& item : _tiles)
  {
    glhckObjectFree(item.second.obj);
  }
  _tiles.clear();
}

void wars::GlhckView::initializeFromGame()
{
  clear();

  std::unordered_map<std::string, Game::Tile> const& tiles = _game->getTiles();
  std::unordered_map<std::string, Game::Unit> const& units = _game->getUnits();
  Rules const& rules = _game->getRules();

  for(auto item : tiles)
  {
    _tiles[item.first] = {item.first, createTileObject(item.second)};
  }

  for(auto item : units)
  {
    _units[item.first] = {item.first, createUnitObject(item.second)};
  }

}

glhckObject* wars::GlhckView::createUnitObject(const wars::Game::Unit& unit)
{
  glhckObject* o = glhckCubeNew(0.5);
  if(!unit.tileId.empty())
  {
    Game::Tile const& tile = _game->getTile(unit.tileId);
    kmVec3 pos = hexToRect({static_cast<kmScalar>(tile.x), static_cast<kmScalar>(tile.y), 0.0f});
    glhckObjectPositionf(o, pos.x, pos.y, pos.z);
  }
  glhckMaterial* m = glhckMaterialNew(nullptr);
  glhckMaterialDiffuseb(m, 255, 0, 0, 255);
  glhckObjectMaterial(o, m);
  glhckMaterialFree(m);
  return o;
}

glhckObject* wars::GlhckView::createTileObject(const wars::Game::Tile& tile)
{
  glhckObject* o = glhckCubeNew(0.5);
  kmVec3 pos = hexToRect({static_cast<kmScalar>(tile.x), static_cast<kmScalar>(tile.y), -1.0f});
  glhckObjectPositionf(o, pos.x, pos.y, pos.z);
  glhckMaterial* m = glhckMaterialNew(nullptr);
  glhckMaterialDiffuseb(m, 0, 255, 0, 255);
  glhckObjectMaterial(o, m);
  glhckMaterialFree(m);
  return o;
}
