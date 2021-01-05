#include <queue>
#include <thread>
#include "tcpsocket.hpp"
#define MAX_QUEUE 10
#define MAX_THREAD 5
typedef void(*handler_t)(int);
typedef void(*TaskHandler_t)(int);
class ThreadTask {
  private:
    int _data;
    TaskHandler_t _handler;
  public:
    ThreadTask(int data, handler_t handler):
      _data(data),_handler(handler){}
    void SetTask(int data, handler_t handler) {
      _data = data;
      _handler = handler;
      return;
    }
    void TaskRun() {
      return _handler(_data);
    }

};

class ThreadPool {
  private:
    std::queue<ThreadTask> _queue;
    int _capacity;
    pthread_mutex_t _mutex;
    pthread_cond_t _cond_pro;
    pthread_cond_t _cond_con;
    int _thr_max;    //用于控制线程的最大数量
  private:
    void thr_start() {
      while(1) {
        pthread_mutex_lock(&_mutex);
        while(_queue.empty()) {
          pthread_cond_wait(&_cond_con, &_mutex);
        }
        ThreadTask tt = _queue.front();
        _queue.pop();
        pthread_mutex_unlock(&_mutex);
        pthread_cond_signal(&_cond_pro);
        tt.TaskRun();
      }
      return;
    }
  public:
    ThreadPool(int maxq = MAX_QUEUE, int maxt = MAX_THREAD):
      _capacity(maxq),_thr_max(maxt) {
        pthread_mutex_init(&_mutex,NULL);
        pthread_cond_init(&_cond_con, NULL);
        pthread_cond_init(&_cond_pro, NULL);
      }

    ~ThreadPool() {
      pthread_mutex_destroy(&_mutex);
      pthread_cond_destroy(&_cond_con);
      pthread_cond_destroy(&_cond_pro);
    }

    bool PoolInit() {
      for(int i = 0; i < _thr_max; i++) {
        std::thread thr(&ThreadPool::thr_start,this);
        thr.detach();
      }
      return true;
    }

    bool TaskPush(ThreadTask &tt) {
      pthread_mutex_lock(&_mutex);
      while(_queue.size() == _capacity) {
        pthread_cond_wait(&_cond_pro, &_mutex);
      }
      _queue.push(tt);
      pthread_mutex_unlock(&_mutex);
      pthread_cond_signal(&_cond_con);
      return true;
    }
};
