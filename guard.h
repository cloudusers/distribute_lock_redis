#ifndef _GUARD_H_
#define _GUARD_H_


#include <stdio.h>
#include <stdlib.h>


class NonCopyable
{
	protected:
		NonCopyable(){}
		~NonCopyable(){}
	private:
		NonCopyable(const NonCopyable &);
		NonCopyable &operator=(const NonCopyable &);
};

//scope lock class
template<class P, class K, class E>
class Guard : NonCopyable 
{
	public:
		Guard(P pointer, K key, E expire) 
			: mutex_(pointer),expire_(expire)
		{
			lockname_ = key + "_yourprojectname";//or lockname_ = key;
			lockvalue_ = pointer->GetGlobalUniqStr();

			while (! mutex_->AcquireLock(lockname_, lockvalue_, expire_)) {
				usleep(100);
			}
		}
		~Guard() 
		{
			if (mutex_) {
				mutex_->ReleaseLock(lockname_, lockvalue_, expire_);
			}
		}
		operator bool() const 
		{
			return (NULL != mutex_);
		}

	private:
		P mutex_;
		K lockname_;
		K lockvalue_;
		E expire_;
};

#endif
