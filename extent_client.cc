// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  srand(clock());
  rclient_port = ((rand() % 32000) | (0x1 << 8));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rclient_port;
  cid = host.str();
  // printf("cid: %s\n", cid.c_str());
  rpcs *rlsrpc = new rpcs(rclient_port);
  rlsrpc->reg(rextent_protocol::invalidate, this, &extent_client::invalidate_handler);
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  ret = cl->call(extent_protocol::create, type, cid, id);
  cache[id] = "";
  meta_cache[id].type = type;
  meta_cache[id].atime = meta_cache[id].ctime = meta_cache[id].mtime = time(NULL);
  meta_cache[id].size = 0;
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here

  if (cache.find(eid) != cache.end()) {
    // printf("Oh, cache hit!\n");
    buf = cache[eid];
    meta_cache[eid].size = buf.size();
    meta_cache[eid].atime = time(NULL);
  } else {
    // printf("Ah, cache miss!\n");
    ret = cl->call(extent_protocol::get, eid, cid, buf);
    cache[eid] = buf;
    meta_cache[eid].size = buf.size();
    meta_cache[eid].atime = time(NULL);
  } 
  // printf("Get finished\n");
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (cache.find(eid) != cache.end()) {
    attr.type = meta_cache[eid].type;
    attr.size = meta_cache[eid].size;
    attr.atime = meta_cache[eid].atime;
    attr.ctime = meta_cache[eid].ctime;
    attr.mtime = meta_cache[eid].mtime;
  } else {
    ret = cl->call(extent_protocol::getattr, eid, cid, attr);
    meta_cache[eid].type = attr.type;
    meta_cache[eid].size = attr.size;
    std::string buf;
    ret = cl->call(extent_protocol::get, eid, std::string(""), buf);
    cache[eid] = buf;
    // std::cout << "Now by the way I will get the data, and here it is: \"" << buf << "\"\n";
  } 

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  if (cache.find(eid) != cache.end()) {
    if (meta_cache.find(eid) == meta_cache.end()) {
      std::cout << "meta ridiculously not cached" << std::endl;
    }
    cache[eid] = buf;
    meta_cache[eid].size = buf.size();
    meta_cache[eid].mtime = meta_cache[eid].ctime = time(NULL);
  } else {
    ret = cl->call(extent_protocol::put, eid, buf, cid, r);
    if (meta_cache.find(eid) != meta_cache.end()) {
      std::cout << "meta ridiculously cached" << std::endl;
    }
    extent_protocol::attr attr;
    cl->call(extent_protocol::getattr, eid, std::string(""), attr);
    cache[eid] = buf;
    meta_cache[eid].size = buf.size();
    meta_cache[eid].mtime = meta_cache[eid].ctime = time(NULL);
    meta_cache[eid].type = attr.type;
    meta_cache[eid].atime = attr.atime;
  }
  
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::remove, eid, cid, r);
  cache.erase(eid);
  meta_cache.erase(eid);
  return ret;
}

rextent_protocol::status
extent_client::invalidate_handler(extent_protocol::extentid_t eid, int &)
{
  rextent_protocol::status rret = rextent_protocol::OK;
  int r;
  cl->call(extent_protocol::put, eid, cache[eid], std::string(""), r);
  cache.erase(eid);
  meta_cache.erase(eid);
  return rret;
}

