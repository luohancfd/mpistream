/**
 * @file mpistream.hpp
 * @author Han Luo (aGFuLmx1b0BnbWFpbC5jb20=)
 * @brief This file is part of the mpistream library.  This library is free
 *        software; you can redistribute it and/or modify it under the
 *        terms of the GNU General Public License as published by the
 *        Free Software Foundation; either version 3, or (at your option)
 *        any later version.
 *        This library is distributed in the hope that it will be useful,
 *        but WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *        GNU General Public License for more details.
 *        You should have received a copy of the GNU General Public License and
 *        a copy of the GCC Runtime Library Exception along with this program;
 *        see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 *        <http://www.gnu.org/licenses/>.
 * @version 1.0
 * @date 2023-06-30
 *
 * @copyright Copyright (c) 2023 Han Luo
 *
 */
#pragma once
#ifndef MPISTREAM_HPP
#define MPISTREAM_HPP
#include <algorithm>
#include <ostream>

#include "mpi.h"

class MPI_filebuf : public std::streambuf {
public:
  using Base = std::streambuf;
  using char_type = typename Base::char_type;
  using int_type = typename Base::int_type;
  using pos_type = typename Base::pos_type;
  using off_type = typename Base::off_type;

private:
  static const std::streamsize buf_size = BUFSIZ;
  char buffer_[buf_size];

  MPI_File fhw;
  bool opened;

  /**
   * @brief Always save one extra space in buffer_
   *        for overflow
   */
  inline void reset_ptr() { setp(buffer_, buffer_ + buf_size - 1); }

protected:
  /**
   * @brief For output streams, this typically results in writing the contents
   * of the put area into the associated sequence, i.e. flushing of the output
   * buffer.
   *
   * @return int Returns ​0​ on success, -1 otherwise. The base class
   * version returns ​0​.
   */
  inline int sync() override {
    int ret = 0;
    if (pbase() < pptr()) {
      const int_type tmp = overflow();
      if (traits_type::eq_int_type(tmp, traits_type::eof())) {
        ret = -1;
      }
    }
    return ret;
  }

  /**
   * @brief Write overflowed chars to file, derived from std::streambuf
   *        It's user's responsibility to maintain correct sequence of
   *        output as we are using shared file pointer
   *
   * @param ch
   * @return int_type Returns unspecified value not equal to Traits::eof() on
   * success, Traits::eof() on failure.
   */
  inline int_type overflow(int_type ch = traits_type::eof()) override {
    // https://stackoverflow.com/a/5647030/8023191
    // https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/fstream.tcc
    int_type ret = traits_type::eof();
    const bool testeof = traits_type::eq_int_type(ch, ret);

    if (pptr() == nullptr) {
      reset_ptr();
      if (!testeof) {
        ret = sputc(ch);
      }
    } else {
      if (!testeof) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
      }
      if (write(pbase(), pptr() - pbase())) {
        ret = traits_type::not_eof(ch);
      }
      reset_ptr();
    }
    return ret;
  }

  /**
   * @brief Writes \c count characters to the output sequence from the character
   * array whose first element is pointed to by \c s . Overwrite this function
   * to achieve no_buffered I/O
   *
   * @param s
   * @param n
   * @return std::streamsize
   */
  inline std::streamsize xsputn(const char_type *s,
                                std::streamsize n) override {
    std::streamsize bufavail = epptr() - pptr();
    std::streamsize ret = n;

    // fill buffer up to "buf_size"
    std::streamsize nfill = std::min(n, bufavail + 1);
    std::copy(s, s + nfill, pptr());
    pbump(nfill); // if nfill == bufavail+1, pptr() == epptr()

    if (nfill == bufavail + 1) {
      // equiv: bufavail + 1<= n
      if (!write(pbase(), pptr() - pbase())) {
        ret = -1;
      } else {
        reset_ptr();
        s += nfill;
        n -= nfill;

        /*
          repeatedly write every chunk until there is
          less data than buf_size - 1
        */
        while (n >= buf_size - 1) {
          write(s, buf_size);
          s += buf_size;
          n -= buf_size;
        }
        std::copy(s, s + n, pptr());
        pbump(n);
      }
    }
    return ret;
  }

  /**
   * @brief Sets the position indicator of the input and/or output
   *        sequence relative to some other position. It will flush
   *        the internal buffer to the file
   * @note  This function is collective, which means seekp(), tellp()
   *        need to be called by all processors
   *
   * @param off relative position to set the position indicator to.
   * @param dir defines base position to apply the relative offset to.
   *            It can be one of the following constants: beg, cur, end
   * @param which
   * @return pos_type The resulting absolute position as defined by the position
   * indicator.
   */
  inline pos_type
  seekoff(off_type off, std::ios_base::seekdir dir,
          __attribute__((__unused__))
          std::ios_base::openmode which = std::ios_base::out) override {
    int ret = pos_type(off_type(-1));
    if (is_open()) {
      int whence;
      if (dir == std::ios_base::beg)
        whence = MPI_SEEK_SET;
      else if (dir == std::ios_base::cur)
        whence = MPI_SEEK_CUR;
      else
        whence = MPI_SEEK_END;

      sync(); /*!< write data to the file */
      if (off != off_type(0) || whence != SEEK_CUR) {
        if (MPI_File_seek_shared(fhw, off, whence)) {
          return ret;
        }
      }
      MPI_Offset tmp;
      MPI_File_get_position_shared(fhw, &tmp);
      /*
        I don't have a better solution
      */
      MPI_Bcast(&tmp, 1, MPI_OFFSET, 0, MPI_COMM_WORLD);
      ret = pos_type(tmp);
    }
    return ret;
  }

  inline pos_type seekpos(pos_type pos, __attribute__((__unused__))
                                        std::ios_base::openmode which =
                                            std::ios_base::out) override {
    return seekoff(off_type(pos), std::ios_base::beg);
  }

  /**
   * @brief Method doing the real writing. It moves the data in the
   *        internal buffer to the file
   *
   * @param pbeg
   * @param nch
   * @return true  Succeed to write
   * @return false Fail to write
   */
  inline bool write(const char_type *pbeg, std::streamsize nch) {
    return nch == 0 ||
           !MPI_File_write_shared(fhw, pbeg, nch, MPI_CHAR, MPI_STATUS_IGNORE);
  }

public:
  MPI_filebuf() : buffer_{}, opened(false) {
    setp(buffer_, buffer_ + buf_size - 1);
  }
  virtual ~MPI_filebuf() override {
    if (opened)
      close();
  };

  /**
   * @brief return nullptr if fail
   *
   * @param file_name
   * @return MPI_filebuf*
   */
  MPI_filebuf *open(const char file_name[]);
  inline bool is_open() const { return opened; }
  MPI_filebuf *close() {
    sync();
    return MPI_File_close(&fhw) ? nullptr : this;
  }

  MPI_File get_fhw() { return fhw; }
#if __cplusplus >= 201103L
  void swap(MPI_filebuf &rhs) { std::swap(*this, rhs); }
#endif
};

#if __cplusplus >= 201103L
inline void swap(MPI_filebuf &lhs, MPI_filebuf &rhs) { lhs.swap(rhs); }
#endif

/* ---------------------------------------------------------------------- */

class mpistream : public std::basic_ostream<char> {
public:
  // Types
  using Base = std::basic_ostream<char>;
  using int_type = typename Base::int_type;
  using char_type = typename Base::char_type;
  using pos_type = typename Base::pos_type;
  using off_type = typename Base::off_type;
  using traits_type = typename Base::traits_type;

  // Non-standard types:
  using filebuf_type = MPI_filebuf;
  using ostream_type = Base;

private:
  filebuf_type filebuf;

public:
  mpistream() : ostream_type(), filebuf() { this->init(&filebuf); }
  mpistream(const char file_name[]) : ostream_type(), filebuf() {
    this->init(&filebuf);
    open(file_name);
  }
  mpistream(const mpistream &) = delete;
  mpistream(mpistream &&__rhs)
      : ostream_type(std::move(__rhs)), filebuf(std::move(__rhs.filebuf)) {
    ostream_type::set_rdbuf(&filebuf);
  }
  ~mpistream() {}

  inline void open(const char file_name[]) {
    if (filebuf.open(file_name) == nullptr) {
      this->setstate(std::ios_base::failbit);
    } else {
      this->clear();
    }
  }
  inline bool is_open() const { return filebuf.is_open(); }
  inline void close() {
    if (!filebuf.close()) {
      this->setstate(ios_base::failbit);
    }
  }

  /*
    Default behaviour from std::ostream
    - operator<<                                                        :
    rdbuf()->sputn
    - basic_ostream& put( char_type ch )                                :
    rdbuf()->sputc
    - basic_ostream& write( const char_type* s, std::streamsize count ) :
    rdbuf()->sputn
    - pos_type tellp()                                                  :
    rdbuf()->pubseekoff
    - basic_ostream& seekp( off_type off, std::ios_base::seekdir dir )  :
    rdbuf()->pubseekpos
    - basic_ostream& flush()                                            :
    rdbuf()->pubsync
  */

#if __cplusplus >= 201103L
  void swap(mpistream &rhs) {
    Base::swap(rhs); /*!< default behavior */
    filebuf.swap(rhs.filebuf);
  }
#endif

  /**
   * @brief Sync shared file pointer to \c processor
   *
   * @param processor
   */
  void sync_shfp(int processor);

  /**
   * @brief MPI collective flush in sequence
   *
   * @return mpistream&
   */
  mpistream &flush_all();

  void write_ordered(const char *ch, std::size_t n_ch);
};

#if __cplusplus >= 201103L
inline void swap(mpistream &lhs, mpistream &rhs) { lhs.swap(rhs); }
#endif

#endif
