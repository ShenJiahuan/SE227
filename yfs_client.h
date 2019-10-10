#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

class yfs_client
{
  extent_client *ec;
  lock_client *lc;

public:
  typedef unsigned long long inum;
  enum xxstatus
  {
    OK,
    RPCERR,
    NOENT,
    IOERR,
    EXIST
  };
  typedef int status;

  struct fileinfo
  {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo
  {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent
  {
    std::string name;
    yfs_client::inum inum;
  };

private:
  static std::string filename(inum);
  static inum n2i(std::string);

public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isfile(inum, bool);
  bool isdir(inum);
  bool isdir(inum, bool);
  bool issymlink(inum);
  bool issymlink(inum, bool);

  int getfile(inum, fileinfo &);
  int getfile(inum, fileinfo &, bool);
  int getdir(inum, dirinfo &);
  int getdir(inum, dirinfo &, bool);

  int setattr(inum, size_t);
  int setattr(inum, size_t, bool);
  int lookup(inum, const char *, bool &, inum &);
  int lookup(inum, const char *, bool &, inum &, bool);
  int create(inum, const char *, mode_t, inum &);
  int create(inum, const char *, mode_t, inum &, bool);
  int readdir(inum, std::list<dirent> &);
  int readdir(inum, std::list<dirent> &, bool);
  int write(inum, size_t, off_t, const char *, size_t &);
  int write(inum, size_t, off_t, const char *, size_t &, bool);
  int read(inum, size_t, off_t, std::string &);
  int read(inum, size_t, off_t, std::string &, bool);
  int unlink(inum, const char *);
  int unlink(inum, const char *, bool);
  int mkdir(inum, const char *, mode_t, inum &);
  int mkdir(inum, const char *, mode_t, inum &, bool);

  /** you may need to add symbolic link related methods here.*/

  int symlink(inum, const char *, const char *, inum &);
  int symlink(inum, const char *, const char *, inum &, bool);
  int readlink(inum, std::string &);
  int readlink(inum, std::string &, bool);
};

#endif
