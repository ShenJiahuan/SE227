// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&mutex, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  bool revoke = false;
  pthread_mutex_lock(&mutex);
  if (acquired[lid]) {
    ret = lock_protocol::RETRY;
    if (std::find(waiting[lid].begin(), waiting[lid].end(), id) == waiting[lid].end()) {
      waiting[lid].push_back(id);
    }
    
    revoke = true;
  } else {
    ret = lock_protocol::OK;
    acquired[lid] = true;
    owner[lid] = id;
  }
  pthread_mutex_unlock(&mutex);
  if (revoke) {
    handle h(owner[lid]);
    rlock_protocol::status rret;
    if (h.safebind()) {
      int r;
      rret = h.safebind()->call(rlock_protocol::revoke, lid, r);
    }
    if (!h.safebind() || rret != rlock_protocol::OK) {
      VERIFY(false);
    }
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  if (!acquired[lid] || owner[lid] != id) {
    pthread_mutex_unlock(&mutex);
    return ret;
  }
  acquired[lid] = false;
  std::deque<std::string> iter_waiting = waiting[lid];
  waiting[lid].clear();
  pthread_mutex_unlock(&mutex);
  for (std::deque<std::string>::iterator iter = iter_waiting.begin(); iter != iter_waiting.end(); iter++) {
    handle h(*iter);
    rlock_protocol::status rret;
    if (h.safebind()) {
      int r;
      rret = h.safebind()->call(rlock_protocol::retry, lid, r);
    }
    if (!h.safebind() || rret != rlock_protocol::OK) {
      VERIFY(false);
    }
  }
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  return lock_protocol::OK;
}

