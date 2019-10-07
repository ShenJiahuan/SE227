// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::n2i(std::string n) {
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum) {
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    return false;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum) {
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool yfs_client::issymlink(inum inum) {
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin) {
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din) {
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

    release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size) {
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    extent_protocol::attr a;
    r = ec->getattr(ino, a);
    if (r != extent_protocol::OK) {
        return r;
    }

    if (a.size == size) {
        return r;
    } else {
        std::string buf;
        r = ec->get(ino, buf);
        if (r != extent_protocol::OK) {
            return r;
        }
        if (a.size < size) {
            buf += std::string(size - a.size, '\0');
        } else {
            buf = buf.substr(0, size);
        }
        r = ec->put(ino, buf);
        if (r != extent_protocol::OK) {
            return r;
        }
    }
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out) {
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool exist;
    lookup(parent, name, exist, ino_out);
    if (exist) {
        return EXIST;
    }

    r = ec->create(extent_protocol::T_FILE, ino_out);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::string buf;

    r = ec->get(parent, buf);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::stringstream ss;
    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK) {
        return r;
    }

    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out) {
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool exist;
    lookup(parent, name, exist, ino_out);
    if (exist) {
        return EXIST;
    }

    r = ec->create(extent_protocol::T_DIR, ino_out);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::string buf;

    r = ec->get(parent, buf);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::stringstream ss;
    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK) {
        return r;
    }

    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out) {
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    if (!isdir(parent)) {
        return IOERR;
    }

    found = false;

    std::list <dirent> list;
    readdir(parent, list);
    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++) {
        if (iter->name == std::string(name)) {
            ino_out = iter->inum;
            found = true;
            break;
        }
    }

    return r;
}

int yfs_client::readdir(inum dir, std::list <dirent> &list) {
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    if (!isdir(dir)) {
        return IOERR;
    }

    std::string data;
    r = ec->get(dir, data);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::istringstream ss(data);
    bool flag = true;
    while (flag) {
        std::string token;
        std::string name;
        yfs_client::inum inum;

        std::getline(ss, token, '/');
        name = token;
        if (token == "") {
            break;
        }

        if (!std::getline(ss, token, '/')) {
            flag = false;
        }
        inum = atoi(token.c_str());

        dirent entry;
        entry.name = name;
        entry.inum = inum;
        list.push_back(entry);
    }

    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data) {
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    if (!isfile(ino)) {
        return IOERR;
    }

    std::string buf;
    r = ec->get(ino, buf);
    if (r != extent_protocol::OK) {
        return r;
    }

    data = buf.substr(off, size);
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written) {
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string buf;

    r = ec->get(ino, buf);

    if (r != extent_protocol::OK) {
        return r;
    }

    std::string buf1;
    buf1.assign(data, size);


    std::string suffix = "";
    if (off + buf1.length() < buf.length()) {
        suffix = buf.substr(off + buf1.length());
    }

    if (off > (int) buf.length()) {
        bytes_written = off - buf.length() + buf1.length();
        buf += std::string(off - buf.length(), '\0');
    } else {
        bytes_written = buf1.length();
        buf = buf.substr(0, off);
    }
    buf += buf1;
    buf += suffix;


    r = ec->put(ino, buf);

    return r;
}

int yfs_client::unlink(inum parent, const char *name) {
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool exist;
    inum ino_out;
    lookup(parent, name, exist, ino_out);
    if (!exist) {
        return NOENT;
    }

    r = ec->remove(ino_out);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::list <dirent> list;
    readdir(parent, list);
    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++) {
        if (iter->name == std::string(name)) {
            list.erase(iter);
            break;
        }
    }

    std::stringstream ss;
    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++) {
        ss << iter->name << "/" << iter->inum << "/";
    }
    ec->put(parent, ss.str());

    return r;
}

int yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino_out) {
    int r = OK;

    r = ec->create(extent_protocol::T_SYMLINK, ino_out);
    if (r != extent_protocol::OK) {
        return r;
    }

    std::stringstream ss;
    ss << link;
    r = ec->put(ino_out, ss.str());
    if (r != extent_protocol::OK) {
        return r;
    }
    std::string buf1;
    ec->get(ino_out, buf1);

    std::string buf;

    r = ec->get(parent, buf);
    if (r != extent_protocol::OK) {
        return r;
    }

    ss.str("");
    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK) {
        return r;
    }

    return r;
}

int yfs_client::readlink(inum sym, std::string &data) {
    int r = OK;
    if (!issymlink(sym)) {
        return IOERR;
    }

    std::string buf;
    r = ec->get(sym, data);
    if (r != extent_protocol::OK) {
        return r;
    }

    return r;
}