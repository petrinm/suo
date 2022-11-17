#pragma once

#include "suo.hpp"

namespace suo {

typedef Block *(*MakeFunction)(const Kwargs &);

class Registry
{
public:
    Registry(const char* name, MakeFunction func);
};

}; // namespace suo
