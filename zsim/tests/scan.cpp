#include <iostream>
#include <stdlib.h>

const int size = 4 * 1024 * 1024;
int array[size];

int main() {

  for (int j = 0; j < 256; j++) {
    for (int i = 0; i < size; i++) {
      array[i] += 1;
    }
  }

  for (int i = 0; i < 8; i++) {
    std::cout << array[rand() % size] << " ";
  }
  std::cout << std::endl;

  return 0;

}
