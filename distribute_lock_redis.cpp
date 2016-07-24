// Copyright 2016 
// License(GPL)
// Author: clouduser@163.com
// This class impl distribute lock of redis


#include "guard.h"
#include "distribute_lock_redis.h"

//for time and thread id
#include <sys/time.h>
#include <pthread.h>

//for get mac addr
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>

using namespace redis3m;

int max_try_count = 5;
#define WAIT_TIME 1000*1000*1

std::string GetMacAddr()
{
	int sockfd;
	struct ifreq tmp;   
	char mac_addr[30];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd < 0) {
		return "x1xxyyyyzzzz";
	}

	memset(&tmp,0,sizeof(struct ifreq));
	strncpy(tmp.ifr_name,"eth0",sizeof(tmp.ifr_name)-1);
	if( (ioctl(sockfd,SIOCGIFHWADDR,&tmp)) < 0 ) {
		return "x2xxyyyyzzzz";
	}

	sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x",
			(unsigned char)tmp.ifr_hwaddr.sa_data[0],
			(unsigned char)tmp.ifr_hwaddr.sa_data[1],
			(unsigned char)tmp.ifr_hwaddr.sa_data[2],
			(unsigned char)tmp.ifr_hwaddr.sa_data[3],
			(unsigned char)tmp.ifr_hwaddr.sa_data[4],
			(unsigned char)tmp.ifr_hwaddr.sa_data[5]
		   );
	close(sockfd);
	std::string mac = mac_addr;
	return mac;
}

/*
 */
DistrLockRedis::DistrLockRedis()
{/*{{{*/
	macaddr = GetMacAddr();
}/*}}}*/

/*
 */
DistrLockRedis::~DistrLockRedis()
{/*{{{*/
}/*}}}*/

/*
 * create redis connection
 */
void DistrLockRedis::Initialize(const std::string& host, const std::string& sentinel, const unsigned int port)
{/*{{{*/
	while (1) {
		try{
			pool = connection_pool::create(host.c_str(), sentinel.c_str(), port);
			break;
		} catch (redis3m::exception &ex) {
			;
		} catch (const unable_to_connect& ex) {
			;
		} catch (...) {
			;
		}
		usleep(WAIT_TIME);
	}
}/*}}}*/

/*
 */
std::string DistrLockRedis::Get(const std::string& key)
{/*{{{*/
	while (1) {
		try{
			connection::ptr_t conn = pool->get(connection::MASTER);
			reply resp = conn->run(command("GET")(key.c_str()));
			pool->put(conn);
			return resp.str();
			/*
			   if(1 == resp.type()) {
			   return resp.str();
			   }
			   */
		} catch (redis3m::exception &ex) {
			;
		} catch (const transport_failure& ex) {
			;
		} catch (...) {
			;
		}

		usleep(WAIT_TIME);
	}
	return "";
}/*}}}*/

/*
 *
 */
bool DistrLockRedis::Set(const std::string& key, const std::string& value)
{/*{{{*/
	while (1) {
		try{
			connection::ptr_t conn = pool->get(connection::MASTER);
			reply resp = conn->run(command("SET")(key.c_str())(value.c_str()));
			pool->put(conn);

			if (5 == resp.type() && resp.str() == "OK") {
				return true;
			}
		} catch (redis3m::exception &ex) {
			;
		} catch (const transport_failure& ex) {
			;
		} catch (...) {
			;
		}

		usleep(WAIT_TIME);
	}
	return false;
}/*}}}*/

/*
 */
bool DistrLockRedis::AcquireLock(std::string lockname, std::string lockvalue, int expire)
{/*{{{*/
	//printf("acquirelock\n");
	while(1) {
		try{
			connection::ptr_t conn = pool->get(connection::MASTER);
			reply resp = conn->run(command("SETNX")(lockname)(lockvalue));
			//printf("key:%s, val:%s, type:%d, integer:%lld , expire:%d\n", lockname.c_str(), lockvalue.c_str(), resp.type(), resp.integer(), expire);
			/*
			   enum type_t
			   {   
			   STRING = 1,
			   ARRAY = 2,
			   INTEGER = 3,
			   NIL = 4,
			   STATUS = 5,
			   ERROR = 6 
			   };
			   */
			if(3 != resp.type()) {
				pool->put(conn);
				//printf("fatal, not interger, \n");
				return false;
			}
			/*
			 * SETNX
			 * ret = 1 should continue your operation
			 * ret = 0 should return, because have been locked
			 */
			if(1 == resp.integer()) {
				resp = conn->run(command("EXPIRE")(lockname)(expire));
				pool->put(conn);
				//printf("setnx succ, integer:%lld, type:%d, expire:%d\n", resp.integer(), resp.type(), expire);
				return true;
			} 
			/*
			 * TTL
			 * -2: key not exist
			 * -1: not set expire
			 * ret = expire time(second)
			 */
			resp = conn->run(command("TTL")(lockname));
			//printf("setnx =0, TTL:%lld\n", resp.integer());
			if(resp.integer() == -1) {
				/* 
				 * there is not set expire(or key not exist), so we should set expire
				 */
				//printf("setnx =0, but not set expire, type:%d, expire:%d\n", resp.type(), expire);
				conn->run(command("EXPIRE")(lockname)(expire));
			}
			if(resp.integer() == -2) {
				//printf("setnx =0, but key not exist\n");
				/*
				 * redis will auto del key after key expire
				 */
			}
			pool->put(conn);
			break;
		} catch (redis3m::exception &ex) {
			;
		} catch (const transport_failure& ex) {
			;
		} catch (...) {
			;
		}

		usleep(WAIT_TIME);
	}

	return false;
}/*}}}*/

/*
 *
 */
bool DistrLockRedis::ReleaseLock(std::string lockname, std::string lockvalue, int expire)
{/*{{{*/
	//printf("releaselock\n");
	while (1) {
		try{
			connection::ptr_t conn = pool->get(connection::MASTER);
			reply resp = conn->run(command("WATCH")(lockname));
			resp = conn->run(command("GET")(lockname));
			if (resp.str() != lockvalue) {
				//printf("ignore release, lockvalue modified, so not to delete lockname=%s,  lockvalue:%s, returnstr:%s\n", lockname.c_str(), lockvalue.c_str(), resp.str().c_str());
				conn->run(command("UNWATCH"));

				pool->put(conn);
				return false;
			}

			resp = conn->run(command("MULTI"));
			resp = conn->run(command("DEL")(lockname));
			resp = conn->run(command("EXEC"));
			//printf("release normal lockname:%s, lockvalue:%s, returnstr:%s\n", lockname.c_str(), lockvalue.c_str(), resp.str().c_str());

			pool->put(conn);
			break;

		} catch (redis3m::exception &ex) {
			;
		} catch (const transport_failure& ex) {
			;
		} catch (...) {
			;
		}

		usleep(WAIT_TIME);
	}

	return true;
}/*}}}*/

/*
 *time + thread_id + mac_addr
 */
std::string DistrLockRedis::GetGlobalUniqStr()
{/*{{{*/
	char buf[64] = {'0'};
	unsigned int ctime = time(NULL);        
	unsigned int tid = (unsigned int)pthread_self();
	sprintf(buf, "%u%u%s", ctime, tid, macaddr.c_str());

	std::string str = buf;
	return str;
}/*}}}*/
