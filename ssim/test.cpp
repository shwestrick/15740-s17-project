#include <iostream>
#include "pccc.hpp"

Memory C = Memory(1);

int main(int argc, char** argv) {
  Cell<int>* x = C.cell<int>();

  x->write(0, 42);
  std::cout << x->read(0) << std::endl;

  x->write(0, 43);
  std::cout << x->read(0) << std::endl;

  return 0;
}
