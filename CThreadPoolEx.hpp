#pragma once

#include <functional>
#include <atlutil.h>

using namespace ATL;

class CThreadInitializeTraits
{
public:
	virtual BOOL Initialize(LPVOID config)
	{
		return TRUE;
	}

	virtual void Terminate(LPVOID config)
	{
	}
};

class CComThreadInitializeTraits
	: public CThreadInitializeTraits
{
public:
	virtual BOOL Initialize(LPVOID config) override
	{
#if defined(_ATL_FREE_THREADED)
		return SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
#else
		return SUCCEEDED(::CoInitialize(nullptr));
#endif
	}

	virtual void Terminate(LPVOID config) override
	{
		::CoUninitialize();
	}
};

class LambdaRequest 
{
private:
	std::function<void()> _call;

public:
	template <typename F>
	LambdaRequest(F&& f)
	{
		auto _f = std::forward<F>(f);
		_call = std::function<void()>([_f]() {
			(_f)();
		});
	}

	template <typename F, typename ... FArgs>
	LambdaRequest(F&& f, FArgs&& ... args)
	{
		auto _f = std::bind(std::forward<F>(f), std::forward<FArgs>(args) ...);
		_call = std::function<void()>([_f]() {
			(_f)();
		});
	}

private:
	LambdaRequest(LambdaRequest& request) = delete;

public:
	void Call(void) const
	{
		_call();
	}
};

template <class TRequest>
class CThreadExecutorTraits
{
public:
	typedef TRequest* RequestType;

	virtual void Execute(RequestType request, LPVOID config, OVERLAPPED* overlapped) throw() = 0;
};

template <class TRequest>
class CThreadLambdaExecutorTraits :
	public CThreadExecutorTraits<TRequest>
{
public:
	virtual void Execute(RequestType request, LPVOID config, OVERLAPPED* overlapped) throw() override
	{
		request->Call();
	}
};

template <class TRequest, class TThreadInitializeTraits = CThreadInitializeTraits, class TThreadExecutorTraits = CThreadExecutorTraits<TRequest>>
class CWorkerArchetype :
	public TThreadInitializeTraits,
	public TThreadExecutorTraits
{
};

template <class TThreadInitializeTraits = CThreadInitializeTraits>
class CLambdaWorkerBase :
	public CWorkerArchetype<LambdaRequest, TThreadInitializeTraits, CThreadLambdaExecutorTraits<LambdaRequest>>
{
};

typedef CLambdaWorkerBase<CThreadInitializeTraits> LambdaWorker;
typedef CLambdaWorkerBase<CComThreadInitializeTraits> ComLambdaWorker;

class CThreadProcHook
{
public:
	static DWORD WorkerThreadProc(LPVOID pv) throw()
	{
		// Get a pointer to the configuration interface.
		IThreadPoolConfig* pConfig = reinterpret_cast<IThreadPoolConfig*>(pv);

		// The dynamic cast is required in order to prevent invalid calls to IUnknown::QueryInterface.
		CThreadProcHook* pThis = dynamic_cast<CThreadProcHook*>(pConfig);

		return pThis->ThreadProc();
	}

protected:
	virtual DWORD ThreadProc() throw() = 0;
};

template <class TThreadTraits = DefaultThreadTraits, class TThreadProcHook = CThreadProcHook>
class ThreadProcHookThreadTraits
{
public:
	static HANDLE CreateThread(
		_In_opt_ LPSECURITY_ATTRIBUTES lpsa,
		_In_ DWORD dwStackSize,
		_In_ LPTHREAD_START_ROUTINE pfnThreadProc,
		_In_opt_ void *pvParam,
		_In_ DWORD dwCreationFlags,
		_Out_opt_ DWORD *pdwThreadId) throw()
	{
		// Ignore the provided worker thread proc function and hook the 
		return TThreadTraits::CreateThread(lpsa, dwStackSize, TThreadProcHook::WorkerThreadProc, pvParam, dwCreationFlags, pdwThreadId);
	}
};

template <class TWorker, class TThreadTraits = ThreadProcHookThreadTraits<DefaultThreadTraits>, class TWaitTraits = DefaultWaitTraits>
class CThreadPoolEx : 
	public CThreadPool<TWorker, TThreadTraits, TWaitTraits>,
	public CThreadProcHook
{
public:
	CThreadPoolEx() throw() :
		CThreadPool()
	{
	}

	virtual ~CThreadPoolEx() throw()
	{
	}

protected:
	virtual DWORD CThreadProcHook::ThreadProc() throw() override
	{
		DWORD dwBytesTransfered;
		ULONG_PTR dwCompletionKey;

		OVERLAPPED* pOverlapped;

		// this block is to ensure theWorker gets destructed before the
		// thread handle is closed
		{
			// We instantiate an instance of the worker class on the stack
			// for the life time of the thread.
			TWorker theWorker;

			if (theWorker.Initialize(m_pvWorkerParam) == FALSE)
			{
				return 1;
			}

			SetEvent(m_hThreadEvent);
			// Get the request from the IO completion port
			while (TRUE)
			{
				// Request the queue status.
				BOOL bStatus = GetQueuedCompletionStatus(m_hRequestQueue, &dwBytesTransfered, &dwCompletionKey, &pOverlapped, INFINITE);

				if (pOverlapped == ATLS_POOL_SHUTDOWN) // Shut down
				{
					LONG bResult = InterlockedExchange(&m_bShutdown, FALSE);
					if (bResult) // Shutdown has not been cancelled
						break;

					// else, shutdown has been cancelled -- continue as before
				}
				else										// Do work
				{
					typename TWorker::RequestType request = (typename TWorker::RequestType) dwCompletionKey;

					// Process the request.  Notice the following:
					// (1) It is the worker's responsibility to free any memory associated
					// with the request if the request is complete
					// (2) If the request still requires some more processing
					// the worker should queue the request again for dispatching
					theWorker.Execute(request, m_pvWorkerParam, pOverlapped);
				}
			}

			theWorker.Terminate(m_pvWorkerParam);
		}

		m_dwThreadEventId = GetCurrentThreadId();
		SetEvent(m_hThreadEvent);

		return 0;
	}
};