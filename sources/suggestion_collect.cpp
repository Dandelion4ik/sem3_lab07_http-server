// Copyright 2020 Kavykin Andrey Kaviandr@yandex.ru

#include "suggestion_collect.hpp"

#include <algorithm>

suggestion_collect::suggestion_collect() { suggestions_ = {}; }
void suggestion_collect::update(json storage) {
  std::sort(storage.begin(), storage.end(),
            [](const json& a, const json& b) -> bool {
              return a.at("cost") < b.at("cost");
            });
  suggestions_ = storage;
}
json suggestion_collect::suggest(const std::string& input) {
  json result;
  for (size_t i = 0, m = 0; i < suggestions_.size(); ++i) {
    if (input == suggestions_[i].at("id")) {
      json pos;
      pos["text"] = suggestions_[i].at("name");
      pos["position"] = m++;
      result["suggestions"].push_back(pos);
    }
  }
  return result;
}
