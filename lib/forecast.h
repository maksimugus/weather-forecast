#include <conio.h>

#include <chrono>
#include <iostream>

#include "cpr/cpr.h"
#include "ftxui/dom/node.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/screen.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum class Button : int {
  esc = 27,
  n = 110,
  p = 112,
  plus = 43,
  minus = 45
};

void GetForecast(int args, char** argv);

