#include <iostream>
#include "pccc.hpp"

int main(int argc, char** argv) {
  Memory M(1);

  Cell<int>* x = M.cell<int>();

  x->write(0, 42);
  std::cout << x->read(0) << std::endl;

  x->write(0, 43);
  std::cout << x->read(0) << std::endl;

  return 0;
}
