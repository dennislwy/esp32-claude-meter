#pragma once
#include <stdint.h>
#include <string>
#include <time.h>

class String : public std::string
{
public:
  using std::string::string;
  String() = default;
  bool isEmpty() const { return empty(); }
};
template<class T> T constrain(T value, T low, T high)
{
  return value < low ? low : value > high ? high : value;
}
uint32_t millis();
struct TestSerial { template<class... T> void printf(const char *, T...) {} };
extern TestSerial Serial;
