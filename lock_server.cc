// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server()
{
  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  if (cond.find(lid) == cond.end())
  {
    pthread_cond_init(&cond[lid], NULL);
  }
  r = nacquire[lid];
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2 part2 code goes here
  printf("acquire request from clt %d\n", clt);
  pthread_mutex_lock(&mutex);

  if (cond.find(lid) == cond.end())
  {
    pthread_cond_init(&cond[lid], NULL);
  }

  while (nacquire[lid] > 0)
  {
    pthread_cond_wait(&cond[lid], &mutex);
  }
  nacquire[lid]++;
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2 part2 code goes here
  printf("release request from clt %d\n", clt);
  pthread_mutex_lock(&mutex);
  nacquire[lid]--;
  pthread_cond_signal(&cond[lid]);
  pthread_mutex_unlock(&mutex);
  return ret;
}
