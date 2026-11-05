#include "conditionvariable.h"

#include "win8sync.h"

namespace dsp56k
{
	namespace
	{
		void cvCpp11NotifyAll(void* _s)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.notify_all();
		}

		void cvCpp11NotifyOne(void* _s)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.notify_one();
		}

		void cvCpp11Wait(void* _s, std::unique_lock<std::mutex>& _lock)
		{
			auto* s = static_cast<ConditionVariableCpp11::Storage*>(_s);
			s->cv.wait(_lock);
		}
#ifdef _WIN32
		void cvWin8NotifyAll(void* _s)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        s->m_waiters.fetch_add(1, std::memory_order_relaxed);
			Win8Sync::getWakeAllFunc()(&s->m_waiters);
		}

		void cvWin8NotifyOne(void* _s)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        s->m_waiters.fetch_add(1, std::memory_order_relaxed);
			Win8Sync::getWakeOneFunc()(&s->m_waiters);
		}

		void cvWin8Wait(void* _s, std::unique_lock<std::mutex>& _lock)
		{
			auto* s = static_cast<ConditionVariableWin8::Storage*>(_s);
	        int oldValue = s->m_waiters.load(std::memory_order_relaxed);
	        _lock.unlock();
			Win8Sync::getWaitOnAddressFunc()(&s->m_waiters, &oldValue, sizeof(oldValue), 0xffffffff);
	        _lock.lock();
		}
#endif		
	}

#ifdef _WIN32
	ConditionVariable::ConditionVariable()
	{
		if (Win8Sync::isSupported())
		{
			m_funcNotifyAll = cvWin8NotifyAll;
			m_funcNotifyOne = cvWin8NotifyOne;
			m_funcWait = cvWin8Wait;

			new (m_storage.data()) ConditionVariableWin8::Storage();
		}
		else
		{
			m_funcNotifyAll = cvCpp11NotifyAll;
			m_funcNotifyOne = cvCpp11NotifyOne;
			m_funcWait = cvCpp11Wait;

			new (m_storage.data()) ConditionVariableCpp11::Storage();
		}
	}

	ConditionVariable::~ConditionVariable()
	{
		if (Win8Sync::isSupported())
			reinterpret_cast<ConditionVariableWin8::Storage*>(m_storage.data())->~Storage();
		else
			reinterpret_cast<ConditionVariableCpp11::Storage*>(m_storage.data())->~Storage();
	}
#endif
}

#ifdef __APPLE__
// Private Darwin futex, available since 10.12 and used internally by libc++/libdispatch. The public
// os_sync_wait_on_address requires macOS 14.4, above our deployment target (11.0).
// MS2000R MAC-LEGACY: weak, so a binary built for OS X 10.9 - 10.11 (which lack them) still loads; there
// ulockAvailable() is false and the mutex/condition-variable fallback below is used.
extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout_us) __attribute__((weak_import));
extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value) __attribute__((weak_import));

namespace dsp56k
{
	namespace
	{
		constexpr uint32_t UL_COMPARE_AND_WAIT	= 1;			// 32-bit compare-and-wait on the futex word
		constexpr uint32_t ULF_WAKE_ALL			= 0x00000100;
		constexpr uint32_t ULF_NO_ERRNO			= 0x01000000;	// return -errno instead of touching errno

		bool ulockAvailable()
		{
			static const bool available = &__ulock_wait != nullptr && &__ulock_wake != nullptr;
			return available;
		}
	}

	void ConditionVariable::notify_one()
	{
		if (!ulockAvailable())
		{
			{ std::lock_guard<std::mutex> l(m_fallbackMutex); m_seq.fetch_add(1, std::memory_order_relaxed); }
			m_fallbackCv.notify_one();
			return;
		}
		m_seq.fetch_add(1, std::memory_order_relaxed);
		__ulock_wake(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, &m_seq, 0);
	}

	void ConditionVariable::notify_all()
	{
		if (!ulockAvailable())
		{
			{ std::lock_guard<std::mutex> l(m_fallbackMutex); m_seq.fetch_add(1, std::memory_order_relaxed); }
			m_fallbackCv.notify_all();
			return;
		}
		m_seq.fetch_add(1, std::memory_order_relaxed);
		__ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL | ULF_NO_ERRNO, &m_seq, 0);
	}

	void ConditionVariable::wait(std::unique_lock<std::mutex>& _lock)
	{
		const uint32_t old = m_seq.load(std::memory_order_relaxed);
		_lock.unlock();
		if (!ulockAvailable())
		{
			// the notifier bumps m_seq under m_fallbackMutex, so a notify between the unlock above and this
			// wait is seen by the predicate - no lost wakeup, as with the futex
			std::unique_lock<std::mutex> l(m_fallbackMutex);
			m_fallbackCv.wait(l, [&] { return m_seq.load(std::memory_order_relaxed) != old; });
			l.unlock();
			_lock.lock();
			return;
		}
		// Block while the futex word is still 'old'. If a notify already bumped it we return immediately,
		// so a wakeup cannot be lost in the window between unlock and wait (the caller re-checks its predicate).
		__ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, &m_seq, old, 0);
		_lock.lock();
	}
}
#endif
