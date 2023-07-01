#include "mpistream.hpp"
#include <iostream>

std::string random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);

  int rank, nrank, buf;

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nrank);

  mpistream of;
  of.open("test.txt");

  // manually control writing sequence
  if (rank != 0)
    MPI_Recv(&buf, 1, MPI_INT, rank - 1, rank - 1, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
  std::string s = random_string(20);
  of << "Rank = " << rank << std::endl;
  of << "Write with operator<<: " << s << '\n';
  of << "Write with write: ";
  of.write(s.c_str(), s.size());
  of.put('\n');
  of.flush();
  if (rank < nrank - 1)
    MPI_Send(&buf, 1, MPI_INT, rank + 1, rank, MPI_COMM_WORLD);

  // get position, collective call
  auto pos = of.tellp();

  // single processor write
  if (rank == 0) {
    of << "Rank = 0 write: !@#$%^&*()";
    of.flush();
  }

  // seek position back
  of.seekp(pos);
  if (rank == 1) {
    of << "Rank = 1 write: helloworld\n";
    of.flush();
  }

  of.flush_all(); // flush all internal buffer in sequence
  of.write_ordered(s.c_str(), 5);

  of.close();

  MPI_Finalize();
  return 0;
}
