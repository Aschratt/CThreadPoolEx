///////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                                                         /////
///// MIT License                                                                             /////
/////                                                                                         /////
///// Copyright(c) 2017 Carsten Rudolph                                                       /////
/////                                                                                         /////
///// Permission is hereby granted, free of charge, to any person obtaining a copy            /////
///// of this software and associated documentation files(the "Software"), to deal            /////
///// in the Software without restriction, including without limitation the rights            /////
///// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell               /////
///// copies of the Software, and to permit persons to whom the Software is                   /////
///// furnished to do so, subject to the following conditions :                               /////
/////                                                                                         /////
///// The above copyright notice and this permission notice shall be included in all          /////
///// copies or substantial portions of the Software.                                         /////
/////                                                                                         /////
///// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR              /////
///// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,                /////
///// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE              /////
///// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER                  /////
///// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,           /////
///// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE           /////
///// SOFTWARE.                                                                               /////
/////                                                                                         /////
///////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                                                         /////
///// Project URL: https://github.com/Aschratt/CThreadPoolEx                                  /////
/////                                                                                         /////
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <atlutil.h>

using namespace ATL;

/// <summary>
/// Provides default initialization and termination methods for the worker archetype.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
class CThreadInitializeTraits
{
public:
	/// <summary>
	/// Called to initialize a new worker thread.
	/// </summary>
	virtual BOOL Initialize(LPVOID config)
	{
		return TRUE;
	}

	/// <summary>
	/// Called when the thread pool is terminating the worker thread.
	/// </summary>
	virtual void Terminate(LPVOID config)
	{
	}
};

/// <summary>
/// Provides initialization and termination methods for the worker archetype, that call <see cref="https://msdn.microsoft.com/en-us/library/windows/desktop/ms695279">`CoInitializeEx`</see> with the current thread model and <see cref="https://msdn.microsoft.com/en-us/library/windows/desktop/ms688715">`CoUninitialize`</see> respectively.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/de-de/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
class CComThreadInitializeTraits
	: public CThreadInitializeTraits
{
public:
	/// <summary>
	/// Called to initialize a new worker thread.
	/// </summary>
	virtual BOOL Initialize(LPVOID config) override
	{
#if defined(_ATL_FREE_THREADED)
		return SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
#else
		return SUCCEEDED(::CoInitialize(nullptr));
#endif
	}

	/// <summary>
	/// Called when the thread pool is terminating the worker thread.
	/// </summary>
	virtual void Terminate(LPVOID config) override
	{
		::CoUninitialize();
	}
};

/// <summary>
/// A request type for the worker archetype that stores a lambda expression, that gets invoked from the worker thread.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/de-de/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
class LambdaRequest 
{
private:
	std::function<void()> _call;

public:
	/// <summary>
	/// Initializes a request based on a simple expression.
	/// </summary>
	template <typename F>
	LambdaRequest(F&& f)
	{
		auto _f = std::forward<F>(f);
		_call = std::function<void()>([_f]() {
			(_f)();
		});
	}

	/// <summary>
	/// Initializes a request based on a expression and a list of arguments.
	/// </summary>
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
	/// <summary>
	/// Invokes the stored expression.
	/// </summary>
	void Invoke(void) const
	{
		_call();
	}
};

/// <summary>
/// Provides an abstract execution method for the worker archetype.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
template <class TRequest>
class CThreadExecutorTraits
{
public:
	typedef TRequest* RequestType;

	/// <summary>
	/// Called by the worker thread to perform a specific task, based on the request parameters provided.
	/// </summary>
	virtual void Execute(RequestType request, LPVOID config, OVERLAPPED* overlapped) throw() = 0;
};

/// <summary>
/// Provides an execution method for the worker archetype, that invokes the expression stored within a <see cref="LambdaRequest">`LambdaRequest`</see>.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
template <class TRequest>
class CThreadLambdaExecutorTraits :
	public CThreadExecutorTraits<TRequest>
{
public:
	/// <summary>
	/// Called by the worker thread to invoke the expression stored within the request parameters.
	/// </summary>
	virtual void Execute(RequestType request, LPVOID config, OVERLAPPED* overlapped) throw() override
	{
		request->Invoke();
	}
};

/// <summary>
/// Describes a default worker archetype implementation, using on a user-defined request type.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
template <class TRequest, class TThreadInitializeTraits = CThreadInitializeTraits, class TThreadExecutorTraits = CThreadExecutorTraits<TRequest>>
class CWorkerArchetype :
	public TThreadInitializeTraits,
	public TThreadExecutorTraits
{
};

/// <summary>
/// Describes a worker archetype implementation, using on an expression-based request type.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
template <class TThreadInitializeTraits = CThreadInitializeTraits>
class CLambdaWorkerBase :
	public CWorkerArchetype<LambdaRequest, TThreadInitializeTraits, CThreadLambdaExecutorTraits<LambdaRequest>>
{
};

/// <summary>
/// Describes a default worker archetype implementation, using on an expression-based request type.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
typedef CLambdaWorkerBase<CThreadInitializeTraits> LambdaWorker;,

/// <summary>
/// Describes a COM worker archetype implementation, using on an expression-based request type.
/// </summary>
///
/// <seealso cref="https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype">Worker Archetype</seealso>
typedef CLambdaWorkerBase<CComThreadInitializeTraits> ComLambdaWorker;

/// <summary>
/// Implement this base class within a custom CThreadPool implementation to support custom delegation to custom `ThreadProc` implementations for worker threads.
/// </summary>
class CThreadProcHook
{
public:
	/// <summary>
	/// Called to initialize a new worker thread.
	/// </summary>
	/// <remarks>
	/// The method retrieves the `IThreadPoolConfiguration` interface, the provided pointer implements, when the thread is created from an `CThreadPool` instance.
	/// It then retrieves a pointer to the current `CThreadProcHook` to directly call the virtual method `ThreadProc` that executes the worker thread.
	///
	/// The casts are required in order to prevent invalid method calls (i.e. to `QueryInterface`) when upcasting the unformatted interface pointer.
	/// </remarks>
	static DWORD WorkerThreadProc(LPVOID pv) throw()
	{
		// Get a pointer to the configuration interface.
		IThreadPoolConfig* pConfig = reinterpret_cast<IThreadPoolConfig*>(pv);

		// The dynamic cast is required in order to prevent invalid calls to IUnknown::QueryInterface.
		CThreadProcHook* pThis = dynamic_cast<CThreadProcHook*>(pConfig);

		return pThis->ThreadProc();
	}

protected:
	/// <summary>
	/// Implement this method to provide worker thread logic.
	/// </summary>
	virtual DWORD ThreadProc() throw() = 0;
};

/// <summary>
/// A class that re-routes the method provided by a `CThreadPool` when a new worker thread is initialized.
/// </summary>
/// <remarks>
/// The default implementation of `CThreadPool` passes the static method `CThreadPool::WorkerThreadProc` from `CThreadPool::InternalResize` to `ThreadTraits::CreateThread` in order to initialize a new worker thread.
/// `CThreadPool::WorkerThreadProc` casts the unformatted source pointer back to `CThreadPool` in order to call `CThreadPool::ThreadProc`, which is protected, but not virtual and therefor cannot be overwritten.
///
/// Use this trait hook on classes that inherit from `CThreadPool` in order to provide custom `ThreadProc` implementations.
/// </remarks>
template <class TThreadTraits = DefaultThreadTraits, class TThreadProcHook = CThreadProcHook>
class ThreadProcHookThreadTraits
{
public:
	/// <summary>
	/// Creates a new worker thread.
	/// </summary>
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

/// <summary>
/// An extented worker thread.
/// </summary>
/// <remarks>
/// The default `CThreadPool` implementation does not correctly remove a thread, if it's handle is closed on application shutdown, i.e. `GetQueuedCompletionStatus` returns `FALSE`.
/// The extented thread pool fixes this issue. *
/// </remarks>
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

				// * actually it does not yet, but I hope it will!
				if (!bStatus)
					break;

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