#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include <deque>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  std::map<lock_protocol::lockid_t, bool> acquired;
  std::map<lock_protocol::lockid_t, std::string> owner;
  std::map<lock_protocol::lockid_t, std::deque<std::string> > waiting;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, pthread_cond_t> cond;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
