#include <iostream>
#include <queue>
#include <pthread.h>
#include <vector>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <sstream>
using namespace std;

//线程数量
#define MAX_THREAD 5
//队列的大小
#define MAX_QUEUE 10

//类型替换  void 函数指针
typedef void (*handler_t)(int);

class ThreadTask
{
    //封装数据和方法
    //初始化
    //设置
    //运行
public:
    ThreadTask(int data,handler_t handle)
        :_data(data)
        ,_handle(handle)
    {}
    void SetTask(int data,handler_t handle)
    {
        _data = data;
        _handle = handle;
        return;
    }
    void TaskRun()
    {
        return _handle(_data);
    }
private:
    int _data;
    handler_t _handle;
};

class ThreadPool
{
private:
    //队列
    queue<ThreadTask> _queue;
    //容量
    int _capacity;
    //锁
    pthread_mutex_t _mutex;
    //两个条件变量
    pthread_cond_t _cond_pro;
    pthread_cond_t _cond_con;
    //线程的最大数量
    int _thr_max; 
private:
    void thr_start()
    {
        //线程入口函数
        //先加锁,然后判断是否有任务
        //    如果有任务: 执行任务
        //          从任务队列中获取一个任务进行处理
        //    没有任务 : 判断是否要退出
        //          是 : 先解锁再退出, 避免程序卡死
        //        不是 : 等待
        while(1)
        {
            pthread_mutex_lock(&_mutex);
            while(_queue.empty())
            {
                pthread_cond_wait(&_cond_con,&_mutex);
            }
            ThreadTask mt = _queue.front();
            _queue.pop();
            pthread_mutex_unlock(&_mutex);
            pthread_cond_signal(&_cond_pro);
            mt.TaskRun();
        }
        //线程安全的出队
        //任务处理应该放在解锁之后,否则同一时间只有一个线程在处理任务
    }

public:
    ThreadPool(int maxq = MAX_QUEUE,int maxt = MAX_THREAD)
        :_capacity(maxq)
        ,_thr_max(maxt)
    {
        //构造函数,初始化线程池
        //初始化互斥锁和条件变量
        pthread_mutex_init(&_mutex,NULL); 
        pthread_cond_init(&_cond_pro,NULL); 
        pthread_cond_init(&_cond_con,NULL); 
    }
    ~ThreadPool()
    {
        //析构函数,销毁互斥锁和条件变量
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond_pro);
        pthread_cond_destroy(&_cond_con);
    }
    bool PoolInit()
    {
        //创建线程
        //当前线程数量++
        //并不关心线程的返回值, 创建完成之后线程分离;
        for(int i = 0;i < _thr_max;i++)
        {
            thread thr(&ThreadPool::thr_start,this);
            thr.detach();
        }
        return true;
    }
    bool TaskPush(ThreadTask &mt)
    {
        //线程安全的任务入队
        pthread_mutex_lock(&_mutex);
        while((int)_queue.size() == _capacity)
        {
            pthread_cond_wait(&_cond_pro,&_mutex); 
        }
        _queue.push(mt);
        pthread_mutex_unlock(&_mutex);
        pthread_cond_signal(&_cond_con);
        return true;
    }
};
