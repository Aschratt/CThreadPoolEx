# CThreadPoolEx

An extended Active Template Library (ATL) thread pool utility with some bug fixes, automatic optional COM un-/initialization and support for C++ lambdas.

The utility can be used together with the default [`ATL::CThreadPool`](https://docs.microsoft.com/en-us/cpp/atl/reference/cthreadpool-class) from the [ATL utilities](https://docs.microsoft.com/en-us/cpp/atl/atl-utilities-reference) to simplify the creation of [worker archetypes](https://docs.microsoft.com/en-us/cpp/atl/reference/worker-archetype). The base class `CWorkerArchetype` can be parameterized with a request type, in order to pass arguments to worker threads.

## Implementation Details

The library separates the initialization and termination logic from the actual execution logic of the worker archetype. It does this by implementing the respective methods in different base classes, called `CThreadInitializeTraits` and `CThreadExecutorTraits`. The default worker archetype can be parameterized with different initialization and execution traits.

Besides the default initialization traits, there's also an implementation that automatically calls `::CoInitializeEx` and `::CoUninitialize` respectively on initialization and termination, depending on the currently defined thread model. If `_ATL_FREE_THREADED` is defined, `::CoInitializeEx` gets called with `COINIT_MULTITHREADED`, in order to set the thread into an multithreaded apartment.

You can use the execution traits to define custom `Execute` implementations. If you implement the default executor traits, you have to override this method in order to provide your thread's logic. However, it is also possible to use the `CThreadLambdaExecutorTraits` together with the `CLambdaRequest` type to use C++11 lambda expressions as worker thread implementations.

There is also a default implementation for lambda requests: Simply use `LambdaWorker` if you do not need COM and `ComLambdaWorker` if you want to enable COM initialization.

## Bug fixes

The default `CThreadPool` implementation has a bug that can occur if your application closes, as described in [here](http://www.win32programmer.info/ATL_Threadpool_socket_server_using_IO_Completion_Ports_problem.html) and [here](http://www.messageloop.info/ASSERT_failed_in_ATL_39_s_CThreadPool_Shutdown.html). The internal thread map does not update correctly, if the thread is closed due to application shutdown. I tried to fix this and was not able to reproduce the issue any longer. However, please not that I am not sure, if my fix or test environment are complete. Feel free to suggest further improvements!


## Example

    #include <stdio>
    #include <CThreadPoolEx.hpp>

    int main(int argc, char* argv[]) {
        // Use this implementation if you work in a COM environment.
        //CThreadPoolEx<CComLambdaWorker> threadPool;
        CThreadPoolEx<CLambdaWorker> threadPool;

        // Start a thread pool with 10 threads.
        threadPool.Initialize(nullptr, 10);

        // Create a set of requests.
        for (int i = 0; i < 100; i++) {
            threadPool.QueueRequest(new LambdaRequest( [](const int& _i) {
                std::cout << "Task " << i << std::endl;
            }, i));
        }

        // Wait for all tasks to finish.
        threadPool.Shutdown(0);
    }