// Copyright 2020 Kavykin Andrey Kaviandr@yandex.ru

#ifndef INCLUDE_JSON_REP_HPP_
#define INCLUDE_JSON_REP_HPP_

#include <nlohmann/json.hpp>
#include <string>
using json = nlohmann::json;

class json_rep {
 public:
  explicit json_rep(const std::string& filename);
  json get_rep() const;
  void load();

 private:
  std::string filename_;
  json storage_;
};

#endif  // INCLUDE_JSON_REP_HPP_
