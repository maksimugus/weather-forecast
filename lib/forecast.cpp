#include "forecast.h"

bool CheckButtons(const json& in, json::iterator& it, int& period) {
  bool update = false;
  if (kbhit()) {
    Button button{getch()};
    switch (button) {
      case Button::n:
        if (it != in.end() - 1) {
          ++it;
          period = (*it)["forecast_days"].get<int>();
          update = true;
        }
        break;
      case Button::p:
        if (it != in.begin()) {
          --it;
          period = (*it)["forecast_days"].get<int>();
          update = true;
        }
        break;
      case Button::plus:
        if (period < 16) {
          ++period;
          update = true;
        }
        break;
      case Button::minus:
        if (period > 1) {
          --period;
          update = true;
        }
        break;
      case Button::esc:
        exit(0);
      default:
        break;
    }
  }
  return update;
}

json GetResponse(const json& config, const json::iterator& it, int period) {
  json data;
  cpr::Response res = cpr::Get(cpr::Url{config["resources"]["api_ninjas"]},
                               cpr::Header{{"X-Api-Key", config["resources"]["api_key"]}},
                               cpr::Parameters{{"name", (*it)["city"]}}
  );

  if (res.status_code == 0 || res.status_code >= 400) {
    std::ifstream temp("../sets/temp.json");
    if (temp) {
      data = json::parse(std::ifstream());
    }
  } else {
    data = json::parse(res.text);
    cpr::Parameters params = {
        {"latitude", std::to_string(data[0]["latitude"].get<double>())},
        {"longitude", std::to_string(data[0]["longitude"].get<double>())},
        {"hourly", "temperature_2m,relativehumidity_2m,weathercode,windspeed_10m"},
        {"current_weather", "true"},
        {"forecast_days", std::to_string(period)},
        {"timezone", "Europe/Moscow"}
    };
    res = cpr::Get(cpr::Url{config["resources"]["open_meteo"]}, params);
    data = json::parse(res.text);
    std::ofstream temp("../sets/temp.json");
    temp << data;
  }
  return data;
}

void Print(const json& data, const json::iterator& it, int period) {
  json t = data["hourly"]["temperature_2m"]; // temperature
  std::string t_u = data["hourly_units"]["temperature_2m"].get<std::string>(); // temperature unit
  json h = data["hourly"]["relativehumidity_2m"]; // humidity
  std::string h_u = data["hourly_units"]["relativehumidity_2m"].get<std::string>(); // humidity unit
  json wc = data["hourly"]["weathercode"]; // weather code
  json ws = data["hourly"]["windspeed_10m"]; // wind speed
  std::string ws_u = data["hourly_units"]["windspeed_10m"].get<std::string>(); // wind speed unit
  json weather_codes = json::parse(std::ifstream("../sets/weather_codes.json"));
  json months_codes = json::parse(std::ifstream("../sets/months_codes.json"));

  std::cout << (*it)["city"].get<std::string>() << '\n'
            << data["current_weather"]["temperature"].get<int>() << " " << t_u << '\n'
            << data["current_weather"]["windspeed"].get<int>() << " " << h_u << '\n'
            << weather_codes[std::to_string(data["current_weather"]["weathercode"].get<int>())].get<std::string>()
            << std::endl;

  for (int i = 0; i < period; ++i) {
    std::string date = data["hourly"]["time"][i * 24].get<std::string>();
    std::cout << date.substr(8, 2) << " of "
              << months_codes[date.substr(5, 2)].get<std::string>()
              << std::endl;
    std::vector<int> time = {
        i * 24 + 6, // morning
        i * 24 + 12, // afternoon
        i * 24 + 18, // evening
        i * 24 + 23 // night
    };

    std::vector<std::vector<std::string>> table_data(5);
    table_data[0] = {"Morning", "Afternoon", "Evening", "Night"};

    for (int k = 0; k < 4; ++k) {
      table_data[1].push_back(std::to_string(t[time[k]].get<int>()) + " " + t_u);
    }

    for (int k = 0; k < 4; ++k) {
      table_data[2].push_back(std::to_string(h[time[k]].get<int>()) + " " + h_u);
    }

    for (int k = 0; k < 4; ++k) {
      table_data[3].push_back(weather_codes[std::to_string(wc[time[k]].get<int>())]);
    }

    for (int k = 0; k < 4; ++k) {
      table_data[4].push_back(std::to_string(ws[time[k]].get<int>()) + " " + ws_u);
    }

    auto table = ftxui::Table(table_data);
    table.SelectAll().Border(ftxui::LIGHT);
    for (int j = 0; j < 4; ++j) {
      table.SelectColumn(j).Border(ftxui::LIGHT);
    }
    table.SelectRow(0).Decorate(ftxui::bold);
    table.SelectRow(0).SeparatorVertical(ftxui::LIGHT);
    table.SelectRow(0).Border(ftxui::DOUBLE);

    auto doc = table.Render();
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fit(doc));
    Render(screen, doc);
    screen.Print();
    std::cout << std::endl;
  }
}

void GetForecast(int args, char** argv) {
  json config = json::parse(std::ifstream("../sets/config.json"));
  json in; // input parameters
  json data; // temporary information

  if (args > 1) {
    for (int i = 1; i < args; ++i) {
      std::string city;
      while (std::isalpha(argv[i][0])) {
        city.append(argv[i]);
        if (std::isalpha(argv[i + 1][0])) {
          city.append(" ");
        }
        ++i;
      }
      data["city"] = city;
      data["forecast_days"] = std::atoi(argv[i++]);
      data["frequency"] = std::atoi(argv[i]);
      in.push_back(data);
    }
  } else {
    in = config["cities"];
  }

  auto it = in.begin();
  int period = (*it)["forecast_days"].get<int>();
  int freq = (*it)["frequency"].get<int>();
  auto start_time = std::chrono::high_resolution_clock::now();

  std::thread t1([&]() {
    while (true) {
      auto cur_time = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(cur_time - start_time).count() % freq == 0) {
        data = GetResponse(config, it, period);
        if (data.empty()) {
          std::cout << "Failed to get weather forecast" << std::endl;
          continue;
        }
        Print(data, it, period);
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  });

  bool update = true;

  std::thread t2([&]() {
    while (true) {
      update = CheckButtons(in, it, period);
      if (update) {
        data = GetResponse(config, it, period);
        if (data.empty()) {
          std::cout << "Failed to get weather forecast" << std::endl;
          continue;
        }
        Print(data, it, period);
        update = false;
      }
    }
  });

  t1.join();
  t2.join();
}