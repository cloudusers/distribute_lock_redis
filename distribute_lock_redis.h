// Copyright 2016 
// License(GPL)
// Author: clouduser@163.com
// This class impl distribute lock of redis
#ifndef _DISTRLOCKREDIS_H_
#define _DISTRLOCKREDIS_H_


#include <iostream>

#include <redis3m/redis3m.hpp>
#include <redis3m/connection_pool.h>

using namespace redis3m;

class DistrLockRedis
{
	public:
		typedef DistrLockRedis* ptr;

		DistrLockRedis();
		virtual ~DistrLockRedis();

	public:
		/**
		*/
		void Initialize(const std::string& host, const std::string& sentinel, const unsigned int port);
		/**
		*/
		std::string Get(const std::string& key);
		/**
		*/
		bool Set(const std::string& key, const std::string& value);

	public:
		/**
		*/
		bool AcquireLock(std::string lockname, std::string lockvalue, int expire);
		/**
		*/
		bool ReleaseLock(std::string lockname, std::string lockvalue, int expire);
		/**
		*/
		std::string GetGlobalUniqStr();

	private:
		connection_pool::ptr_t pool;
		std::string macaddr;
};

#endif
