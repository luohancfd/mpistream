# mpistream

A C++11 class enables using methods similar to the ones of `std::ofstream` but writing to MPI output with shared file pointer.

## Components

- `MPI_filebuf : public std::streambuf`: The file buffer doing caching and real outputting
- `mpistream : public std::basic_ostream<char>`: Providing high level output operations on the `MPI_filebuf` stream

## Reference

1. std::basic_ostream: https://en.cppreference.com/w/cpp/io/basic_ostream
2. MPI shared file pointer: https://www.mpi-forum.org/docs/mpi-2.2/mpi22-report/node283.htm
3. libstdc++ fstream implementation: https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/fstream.tcc

## LICENSE

GPL v3.0

## Author

Han Luo @ aGFuLmx1b0BnbWFpbC5jb20=

