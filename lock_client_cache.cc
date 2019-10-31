// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <unistd.h>
#include "tprintf.h"

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu)
{
  srand(time(NULL) ^ last_port);
  rlock_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  bool retry = true;
  while (retry) {
    retry = true;
    pthread_mutex_lock(&mutex);
    if (locks.find(lid) == locks.end()) {
      locks[lid] = std::make_pair(NONE, pthread_self());
    }
    if (cond.find(lid) == cond.end()) {
      pthread_cond_init(&cond[lid], NULL);
    }
    if (retry_cond.find(lid) == retry_cond.end()) {
      pthread_cond_init(&retry_cond[lid], NULL);
    }
    n_threads_waiting[lid].erase(pthread_self());
    if (n_threads_waiting[lid].size() == 0) {
      retry_handled[lid] = false;
    }
    while (
      locks[lid].first == LOCKED || 
      locks[lid].first == ACQUIRING || 
      locks[lid].first == RELEASING
    ) {
      pthread_cond_wait(&cond[lid], &mutex);
    }
    VERIFY(locks[lid].first != LOCKED &&
      locks[lid].first != ACQUIRING &&
      locks[lid].first != RELEASING);
    if (locks[lid].first == FREE) {
      locks[lid] = std::make_pair(LOCKED, pthread_self());
      retry = false;
    } else if (locks[lid].first == NONE) {
      locks[lid] = std::make_pair(ACQUIRING, pthread_self());
      n_threads_waiting[lid].insert(pthread_self());
      pthread_mutex_unlock(&mutex);
      int r;
      lock_protocol::status ret = cl->call(lock_protocol::acquire, lid, id, r);
      pthread_mutex_lock(&mutex);
      if (ret == lock_protocol::OK) {
        locks[lid] = std::make_pair(LOCKED, pthread_self());
        n_threads_waiting[lid].erase(pthread_self());
        retry = false;
      } else if (ret == lock_protocol::RETRY) {
        locks[lid] = std::make_pair(NONE, pthread_self());
        pthread_cond_broadcast(&cond[lid]);
        if (!retry_handled[lid]) {
          pthread_cond_wait(&retry_cond[lid], &mutex);
        }
      } else {
        VERIFY(false);
      }
    }
    pthread_mutex_unlock(&mutex);
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
  pthread_mutex_lock(&mutex);
  VERIFY(locks[lid].first == LOCKED);
  if (revoked[lid] && n_threads_waiting[lid].size() == 0) {
    locks[lid] = std::make_pair(lock_client_cache::RELEASING, pthread_self());
    pthread_mutex_unlock(&mutex);
    int r;
    cl->call(lock_protocol::release, lid, id, r);
    VERIFY(revoked[lid] && n_threads_waiting[lid].size() == 0);
    pthread_mutex_lock(&mutex);
    revoked[lid] = false;
    locks[lid] = std::make_pair(lock_client_cache::NONE, pthread_self());
    pthread_cond_broadcast(&cond[lid]);
  } else if (n_threads_waiting[lid].size() != 0) {
    locks[lid] = std::make_pair(lock_client_cache::FREE, pthread_self());
    pthread_cond_broadcast(&retry_cond[lid]);
  } else if (!revoked[lid]) {
    locks[lid] = std::make_pair(lock_client_cache::FREE, pthread_self());
    pthread_cond_broadcast(&cond[lid]);
  } else {
    VERIFY(false);
  }
  pthread_mutex_unlock(&mutex);
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &) {
  usleep(1000);
  pthread_mutex_lock(&mutex);
  if (!revoked[lid]) {
    if (locks[lid].first != FREE) {
      revoked[lid] = true;
    } else {
      locks[lid] = std::make_pair(lock_client_cache::RELEASING, pthread_self());
      pthread_mutex_unlock(&mutex);
      int r;
      cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);
      locks[lid] = std::make_pair(lock_client_cache::NONE, pthread_self());
      pthread_cond_broadcast(&cond[lid]);
    }
  }
  pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &) {
  pthread_mutex_lock(&mutex);
  VERIFY(retry_cond.find(lid) != retry_cond.end());
  if (n_threads_waiting[lid].size() > 0) {
    retry_handled[lid] = true;
    pthread_cond_broadcast(&retry_cond[lid]);
  }
  pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}