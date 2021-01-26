// Copyright 2020 Kavykin Andrey Kaviandr@yandex.ru

#ifndef INCLUDE_SUGGESTION_COLLECT_HPP_
#define INCLUDE_SUGGESTION_COLLECT_HPP_
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class suggestion_collect {
 public:
  suggestion_collect();
  void update(json storage);
  json suggest(const std::string& input);

 private:
  json suggestions_;
};

#endif  // INCLUDE_SUGGESTION_COLLECT_HPP_
