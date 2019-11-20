// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server::extent_server() 
{
  im = new inode_manager();
  // pthread_mutex_init(&mutex, NULL);
}

int extent_server::create(uint32_t type, std::string cid, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  // pthread_mutex_lock(&mutex);
  // printf("extent_server: create inode\n");
  id = im->alloc_inode(type);
  cache_owner[id] = cid;
  // pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, std::string cid, int &)
{
  // pthread_mutex_lock(&mutex);
  if (cid == "") {
    // pthread_mutex_unlock(&mutex);
    // such call is invoked by a client who is invalidating its cache
  } else if (cache_owner.find(id) != cache_owner.end() && cache_owner[id] != cid) {
    // pthread_mutex_unlock(&mutex);
    // printf("Now invalidating %llu: %s's ownership by put\n", id, cache_owner[id].c_str());
    invalidate(id, cache_owner[id]);
    // pthread_mutex_lock(&mutex);
    // printf("put setting owner of %llu to %s\n", id, cid.c_str());
    cache_owner[id] = cid;
  } else if (cache_owner[id] == cid) {
    // pthread_mutex_unlock(&mutex);
    // already owner, why not put in cache?
    VERIFY(false);
  }

  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  if (cid != "") {
    cache_owner[id] = cid;
  }
  // pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string cid, std::string &buf)
{
  // pthread_mutex_lock(&mutex);
  if (cid == "") {
    // get after getattr
  } else if (cache_owner.find(id) != cache_owner.end() && cache_owner[id] != cid) {
    // pthread_mutex_unlock(&mutex);
    // printf("Now invalidating %lld: %s's ownership by get\n", id, cache_owner[id].c_str());
    invalidate(id, cache_owner[id]);
    // pthread_mutex_lock(&mutex);
    // printf("get setting owner of %llu to %s\n", id, cid.c_str());
    cache_owner[id] = cid;
  } else if (cache_owner[id] == cid) {
    // pthread_mutex_unlock(&mutex);
    // already owner, why not put in cache?
    VERIFY(false);
  }
  // printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }
  if (cid != "") {
    cache_owner[id] = cid;
  }
  // pthread_mutex_unlock(&mutex);

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, std::string cid, extent_protocol::attr &a)
{
  // pthread_mutex_lock(&mutex);
  if (cid == "") {
    // getattr after put
  } else if (cache_owner.find(id) != cache_owner.end() && cache_owner[id] != cid) {
    // pthread_mutex_unlock(&mutex);
    // printf("Now invalidating %llu: %s's ownership by getattr\n", id, cache_owner[id].c_str());
    invalidate(id, cache_owner[id]);
    // pthread_mutex_lock(&mutex);
    // printf("getattr setting owner of %llu to %s\n", id, cid.c_str());
    cache_owner[id] = cid;
  } else if (cache_owner[id] == cid) {
    // pthread_mutex_unlock(&mutex);
    // already owner, why not put in cache?
    // printf("%llu already belongs to %s\n", id, cid.c_str());
    VERIFY(false);
  }
  // printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;
  // pthread_mutex_unlock(&mutex);

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, std::string cid, int &)
{
  // pthread_mutex_lock(&mutex);
  if (cache_owner.find(id) != cache_owner.end() && cache_owner[id] != cid) {
    // pthread_mutex_unlock(&mutex);
    // printf("Now invalidating %llu: %s's ownership by remove\n", id, cache_owner[id].c_str());
    invalidate(id, cache_owner[id]);
    // pthread_mutex_lock(&mutex);
    cache_owner.erase(id);
  } else if (cache_owner[id] == cid) {
    cache_owner.erase(id);
  }
  // printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
  // pthread_mutex_unlock(&mutex);
  return extent_protocol::OK;
}

int extent_server::invalidate(extent_protocol::extentid_t id, std::string cid) {
  handle h(cid);
  rextent_protocol::status rret;
  if (h.safebind()) {
    int r;
    rret = h.safebind()->call(rextent_protocol::invalidate, id, r);
  }
  if (!h.safebind() || rret != rextent_protocol::OK) {
    VERIFY(false);
  }
  return extent_protocol::OK;
}