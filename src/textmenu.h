#ifndef TEXTMENU_H
#define TEXTMENU_H

#include "glhck/glhck.h"
#include <map>
#include <string>

class TextMenu
{
public:
  TextMenu();
  ~TextMenu();
  void addOption(int value, std::string const& label, int order = 0);
  template<typename T> void addOption(T value, std::string const& label, int order = 0);
  void clear();
  void update();
  bool input(std::string const& c, int* result);
  bool input(int key, int* result);
  void render();
private:
  struct Entry
  {
    int value;
    std::string label;
  };
  glhckText* text;
  unsigned int font;
  std::multimap<int, Entry> entries;
  static std::string const KEYS;
};

template<typename T> void TextMenu::addOption(T value, std::string const& label, int order)
{
  addOption(static_cast<int>(value), label, order);
}

#endif // TEXTMENU_H
