#include <iostream>
#include <print>

import nm.engine;

auto main() -> int {
  nm::Engine engine{{
      .width = 800,
      .height = 600,
      .application_name = "Not Minecraft",
  }};

  try {
    engine.Run();
  } catch (const std::exception& e) {
    std::println(std::cerr, "{}", e.what());
    return -1;
  }

  return 0;
}
