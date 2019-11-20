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
  lc = new lock_client_cache(lock_dst);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    return isfile(inum, true);
}

bool yfs_client::isfile(inum inum, bool lock)
{
    if (lock)
    {
        lc->acquire(inum);
    }
    bool r;
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        r = false;
        goto release;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        printf("isfile: %lld is a file\n", inum);
        r = true;
        goto release;
    }
    printf("isfile: %lld is not a file\n", inum);
    r = false;
release:
    if (lock)
    {
        lc->release(inum);
    }
    return r;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum)
{
    return isdir(inum, true);
}

bool yfs_client::isdir(inum inum, bool lock)
{
    if (lock)
    {
        lc->acquire(inum);
    }
    bool r;
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        r = false;
        goto release;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        printf("isdir: %lld is a dir\n", inum);
        r = true;
        goto release;
    }
    printf("isdir: %lld is not a dir\n", inum);
    r = false;
release:
    if (lock)
    {
        lc->release(inum);
    }
    return r;
}

bool yfs_client::issymlink(inum inum)
{
    return issymlink(inum, true);
}

bool yfs_client::issymlink(inum inum, bool lock)
{
    if (lock)
    {
        lc->acquire(inum);
    }
    bool r;
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        r = false;
        goto release;
    }

    if (a.type == extent_protocol::T_SYMLINK)
    {
        printf("issymlink: %lld is a symlink\n", inum);
        r = true;
        goto release;
    }
    printf("issymlink: %lld is not a symlink\n", inum);
    r = false;
release:
    if (lock)
    {
        lc->release(inum);
    }
    return r;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    return getfile(inum, fin, true);
}

int yfs_client::getfile(inum inum, fileinfo &fin, bool lock)
{
    if (lock)
    {
        lc->acquire(inum);
    }

    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    if (lock)
    {
        lc->release(inum);
    }
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    return getdir(inum, din, true);
}

int yfs_client::getdir(inum inum, dirinfo &din, bool lock)
{
    if (lock)
    {
        lc->acquire(inum);
        printf("getdir acquire %lld\n", inum);
    }
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    if (lock)
    {
        lc->release(inum);
        printf("getdir release %lld\n", inum);
    }
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
int yfs_client::setattr(inum ino, size_t size)
{
    return setattr(ino, size, true);
}

int yfs_client::setattr(inum ino, size_t size, bool lock)
{
    if (lock)
    {
        lc->acquire(ino);
    }
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    extent_protocol::attr a;
    r = ec->getattr(ino, a);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    if (a.size == size)
    {
        goto release;
    }
    else
    {
        std::string buf;
        r = ec->get(ino, buf);
        if (r != extent_protocol::OK)
        {
            goto release;
        }
        if (a.size < size)
        {
            buf += std::string(size - a.size, '\0');
        }
        else
        {
            buf = buf.substr(0, size);
        }
        r = ec->put(ino, buf);
        if (r != extent_protocol::OK)
        {
            goto release;
        }
    }
release:
    if (lock)
    {
        lc->release(ino);
    }
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return create(parent, name, mode, ino_out, true);
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, bool lock)
{
    std::cout << "trying to create" << std::endl;
    if (lock)
    {
        lc->acquire(parent);
    }
    int r = OK;
    std::stringstream ss;
    std::string buf;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool exist;
    lookup(parent, name, exist, ino_out, false);
    if (exist)
    {
        r = EXIST;
        goto release;
    }

    std::cout << "create file@@@@" << std::endl;
    r = ec->create(extent_protocol::T_FILE, ino_out);
    if (lock)
    {
        lc->acquire(ino_out);
    }
    if (r != extent_protocol::OK)
    {
        goto release;
    }
    std::cout << "parent: " << parent << " ino_out: " << ino_out << std::endl;
    r = ec->get(parent, buf);
    std::cout << "finish get parent" << std::endl;
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK)
    {
        goto release;
    }

release:
    if (lock)
    {
        if (!exist)
        {
            lc->release(ino_out);
        }

        lc->release(parent);
    }
    std::cout << "@@@finish create file" << std::endl;
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return mkdir(parent, name, mode, ino_out, true);
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out, bool lock)
{
    printf("trying to acquire for %lld\n", parent);
    if (lock)
    {
        lc->acquire(parent);
    }

    printf("acquired for %lld\n", parent);
    int r = OK;
    std::string buf;
    std::stringstream ss;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool exist;
    lookup(parent, name, exist, ino_out, false);

    printf("acquired for %lld\n", ino_out);

    if (exist)
    {
        r = EXIST;
        goto release;
    }

    r = ec->create(extent_protocol::T_DIR, ino_out);
    if (lock)
    {
        lc->acquire(ino_out);
    }
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    r = ec->get(parent, buf);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK)
    {
        goto release;
    }

release:
    if (lock)
    {
        if (!exist)
        {
            lc->release(ino_out);
        }

        lc->release(parent);
        printf("release %lld and %lld\n", ino_out, parent);
    }
    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    return lookup(parent, name, found, ino_out, true);
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out, bool lock)
{
    found = false;
    std::cout << "###lookup###" << std::endl;
    if (lock)
    {
        lc->acquire(parent);
    }
    int r = OK;
    std::list<dirent> list;
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    if (!isdir(parent, false))
    {
        r = IOERR;
        goto release;
    }

    readdir(parent, list, false);
    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        if (iter->name == std::string(name))
        {
            ino_out = iter->inum;
            found = true;
            break;
        }
    }

release:
    if (lock)
    {
        lc->release(parent);
    }
    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    return readdir(dir, list, true);
}

int yfs_client::readdir(inum dir, std::list<dirent> &list, bool lock)
{
    if (lock)
    {
        lc->acquire(dir);
    }
    int r = OK;
    std::istringstream ss;
    std::string data;
    bool flag = true;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    if (!isdir(dir, false))
    {
        r = IOERR;
        goto release;
    }

    r = ec->get(dir, data);
    ss.str(data);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    while (flag)
    {
        std::string token;
        std::string name;
        yfs_client::inum inum;

        std::getline(ss, token, '/');
        name = token;
        if (token == "")
        {
            break;
        }

        if (!std::getline(ss, token, '/'))
        {
            flag = false;
        }
        inum = atoi(token.c_str());

        dirent entry;
        entry.name = name;
        entry.inum = inum;
        list.push_back(entry);
    }
release:
    if (lock)
    {
        lc->release(dir);
    }
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    return read(ino, size, off, data, true);
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data, bool lock)
{
    if (lock)
    {
        lc->acquire(ino);
    }
    int r = OK;
    std::string buf;
    /*
     * your code goes here.
     * note: read using ec->get().
     */

    if (!isfile(ino, false))
    {
        r = IOERR;
        goto release;
    }

    r = ec->get(ino, buf);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    data = buf.substr(off, size);
release:
    if (lock)
    {
        lc->release(ino);
    }
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{
    return write(ino, size, off, data, bytes_written, true);
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written, bool lock)
{
    if (lock)
    {
        lc->acquire(ino);
    }
    int r = OK;
    std::string buf;
    std::string buf1;
    std::string suffix = "";
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    r = ec->get(ino, buf);

    if (r != extent_protocol::OK)
    {
        goto release;
    }

    buf1.assign(data, size);

    if (off + buf1.length() < buf.length())
    {
        suffix = buf.substr(off + buf1.length());
    }

    if (off > (int)buf.length())
    {
        buf += std::string(off - buf.length(), '\0');
    }
    else
    {
        buf = buf.substr(0, off);
    }
    bytes_written = buf1.length();
    buf += buf1;
    buf += suffix;

    r = ec->put(ino, buf);
release:
    if (lock)
    {
        lc->release(ino);
    }
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    return unlink(parent, name, true);
}

int yfs_client::unlink(inum parent, const char *name, bool lock)
{
    if (lock)
    {
        lc->acquire(parent);
    }
    int r = OK;
    std::list<dirent> list;
    std::stringstream ss;
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool exist;
    inum ino_out;
    lookup(parent, name, exist, ino_out, false);
    if (!exist)
    {
        r = NOENT;
        goto release;
    }
    if (lock)
    {
        lc->acquire(ino_out);
    }

    r = ec->remove(ino_out);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    readdir(parent, list, false);
    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        if (iter->name == std::string(name))
        {
            list.erase(iter);
            break;
        }
    }

    for (std::list<dirent>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        ss << iter->name << "/" << iter->inum << "/";
    }
    ec->put(parent, ss.str());

release:
    if (lock)
    {
        if (exist)
        {
            lc->release(ino_out);
        }
        lc->release(parent);
    }
    return r;
}

int yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino_out)
{
    return symlink(parent, link, name, ino_out, true);
}

int yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino_out, bool lock)
{
    if (lock)
    {
        lc->acquire(parent);
    }

    int r = OK;
    std::stringstream ss;
    std::string buf1;
    std::string buf;

    r = ec->create(extent_protocol::T_SYMLINK, ino_out);
    if (lock)
    {
        lc->acquire(ino_out);
    }
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    ss << link;
    r = ec->put(ino_out, ss.str());
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    ec->get(ino_out, buf1);

    r = ec->get(parent, buf);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

    ss.str("");
    ss << buf << name << "/" << ino_out << "/";

    r = ec->put(parent, ss.str());
    if (r != extent_protocol::OK)
    {
        goto release;
    }

release:
    if (lock)
    {
        lc->release(ino_out);
        lc->release(parent);
    }
    return r;
}

int yfs_client::readlink(inum sym, std::string &data)
{
    return readlink(sym, data, true);
}

int yfs_client::readlink(inum sym, std::string &data, bool lock)
{
    if (lock)
    {
        lc->acquire(sym);
    }
    int r = OK;
    std::string buf;

    if (!issymlink(sym, false))
    {
        r = IOERR;
        goto release;
    }

    r = ec->get(sym, data);
    if (r != extent_protocol::OK)
    {
        goto release;
    }

release:
    if (lock)
    {
        lc->release(sym);
    }
    return r;
}