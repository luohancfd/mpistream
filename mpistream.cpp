#include "mpistream.hpp"

MPI_filebuf* MPI_filebuf::open(const char file_name[]) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) {
    remove(file_name);
  }
  int err = MPI_File_open(MPI_COMM_WORLD, file_name,
                          MPI_MODE_CREATE | MPI_MODE_EXCL | MPI_MODE_WRONLY,
                          MPI_INFO_NULL, &fhw);
  if (err != MPI_SUCCESS) {
    if (rank == 0) MPI_File_delete(file_name, MPI_INFO_NULL);
    err = MPI_File_open(MPI_COMM_WORLD, file_name,
                        MPI_MODE_CREATE | MPI_MODE_EXCL | MPI_MODE_WRONLY,
                        MPI_INFO_NULL, &fhw);
  }
  if (err) return nullptr;
  opened = true;
  return this;
}

/* ---------------------------------------------------------------------- */

void mpistream::sync_shfp(int processor) {
  unsigned long pos = this->tellp();
  MPI_Bcast(&pos, 1, MPI_UNSIGNED_LONG, processor, MPI_COMM_WORLD);
  this->seekp(pos);
}

/* ---------------------------------------------------------------------- */

mpistream& mpistream::flush_all() {
  int rank, np;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  if (rank != 0)
    MPI_Recv(&np, 1, MPI_INT, rank - 1, rank - 1, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
  flush();
  if (rank < np - 1) MPI_Send(&np, 1, MPI_INT, rank + 1, rank, MPI_COMM_WORLD);
  return *this;
}

/* ---------------------------------------------------------------------- */

void mpistream::write_ordered(const char* ch, std::size_t n_ch) {
  flush_all();
  MPI_File_write_ordered(filebuf.get_fhw(), ch, n_ch, MPI_CHAR,
                         MPI_STATUS_IGNORE);
}
