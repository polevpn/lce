#ifndef __NCE_CLOCK_H
#define __NCE_CLOCK_H

#include <pthread.h>

namespace lce
{

	class CLock
	{
	public:
		CLock(void)
		{
		}

		virtual bool lock() = 0;
		virtual bool unlock() = 0;
		virtual bool tryLock() = 0;

		virtual ~CLock(void)
		{
		}
	};

	class CMutex : public CLock
	{
	public:
		friend class Condition;
		inline CMutex(void)
		{
			::pthread_mutex_init(&m_lock, 0);
		}
		inline ~CMutex(void)
		{
			::pthread_mutex_destroy(&m_lock);
		}

		inline virtual bool lock()
		{

			::pthread_mutex_lock(&m_lock);
			return true;
		};

		inline virtual bool tryLock()
		{
			return 0 == ::pthread_mutex_trylock(&m_lock);
		}

		inline virtual bool unlock()
		{
			::pthread_mutex_unlock(&m_lock);
			return true;
		}

	private:
		CMutex(const CMutex &rhs);
		CMutex &operator=(const CMutex &rhs);

	private:
		char m_szErrMsg[1024];
		pthread_mutex_t m_lock;
	};

	class CReadWriteLock
	{
	private:
		pthread_rwlock_t m_sect;

		CReadWriteLock(const CReadWriteLock &);
		CReadWriteLock &operator=(const CReadWriteLock &);

	public:
		CReadWriteLock() { ::pthread_rwlock_init(&m_sect, NULL); }
		~CReadWriteLock() { ::pthread_rwlock_destroy(&m_sect); }

		bool readLock() { return 0 == ::pthread_rwlock_rdlock(&m_sect); }
		bool writeLock() { return 0 == ::pthread_rwlock_wrlock(&m_sect); }
		bool tryReadLock() { return 0 == ::pthread_rwlock_tryrdlock(&m_sect); }
		bool tryWriteLock() { return 0 == ::pthread_rwlock_trywrlock(&m_sect); }
		bool unlock() { return 0 == ::pthread_rwlock_unlock(&m_sect); }
	};

	class CSpinLock : public CLock
	{
	private:
		pthread_spinlock_t m_sect;
		CSpinLock(const CSpinLock &);
		CSpinLock &operator=(const CSpinLock &);

	public:
		explicit CSpinLock(int pshared = PTHREAD_PROCESS_PRIVATE)
		{
			::pthread_spin_init(&m_sect, pshared);
		}
		~CSpinLock()
		{
			::pthread_spin_destroy(&m_sect);
		}
		inline virtual bool lock() { return 0 == ::pthread_spin_lock(&m_sect); }
		inline virtual bool tryLock() { return 0 == ::pthread_spin_trylock(&m_sect); }
		inline virtual bool unlock() { return 0 == ::pthread_spin_unlock(&m_sect); }
	};

	class Condition
	{
		pthread_cond_t m_sect;

		Condition(const Condition &);
		Condition &operator=(const Condition &);

	public:
		Condition() { ::pthread_cond_init(&m_sect, NULL); }
		~Condition() { ::pthread_cond_destroy(&m_sect); }
		bool signal() { return 0 == ::pthread_cond_signal(&m_sect); }
		bool broadcast() { return 0 == ::pthread_cond_broadcast(&m_sect); }
		bool wait(CMutex &m) { return 0 == ::pthread_cond_wait(&m_sect, &m.m_lock); }
		bool wait(CMutex &m, int iWaitTime)
		{
			struct timespec tv;
			clock_gettime(CLOCK_MONOTONIC, &tv);
			tv.tv_sec += iWaitTime / 1000;
			tv.tv_nsec += (iWaitTime % 1000) * 1000000;

			return 0 == ::pthread_cond_timedwait(&m_sect, &m.m_lock, &tv);
		}
	};

	class CAutoLock
	{
	public:
		CAutoLock(CLock &lock)
			: m_lock(lock)
		{
			m_lock.lock();
		}

		virtual ~CAutoLock(void)
		{
			m_lock.unlock();
		}

	private:
		CLock &m_lock;
	};

};

#endif
