/*
 * 实现server类，完成服务端的整体结构流程
 */
#include "tcpsocket.hpp"
#include "epollwait.hpp"
#include "threadpool.hpp"
#include "http.hpp"
class Server {
  public:
    bool Start(int port) {
      bool ret;
      ret = _lst_sock.SocketInit(port);
      if(ret == false) {
        return false;
      }
      ret = _epoll.Init();
      if(ret == false) {
        return false;
      }
      ret = _pool.PoolInit();
      if(ret == false) {
        return false;
      }
      _epoll.Add(_lst_sock);
      while(1) {
        std::vector<TcpSocket> list;
        ret = _epoll.Wait(list);
        if(ret == false) {
          continue;
        }
        for(int i = 0; i < list.size(); i++) {
          if(list[i].GetFd() == _lst_sock.GetFd()) {
            TcpSocket cli_sock;
            ret = _lst_sock.Accept(cli_sock);
            if(ret == false) {
              continue;
            }
            cli_sock.SetNonBlock();
            _epoll.Add(cli_sock);
          } 
          else {
            ThreadTask tt(list[i].GetFd(),ThreadHandler);
            _pool.TaskPush(tt);
            _epoll.Del(list[i]);
          }
        }
      }
      _lst_sock.Close();
      return true;
    }   

  public:
    static void ThreadHandler(int sockfd) {
      TcpSocket sock;
      sock.SetFd(sockfd);
      HttpRequest req;
      HttpResponse rsp;
      // 从cli_sock接受请求数据，进行解析
      int status = req.RequestParse(sock);
      if(status != 200) {
        // 解析失败直接响应错误
        rsp._status = status; 
        rsp.ErrorProcess(sock);
        sock.Close();
        return;
      }
      // 根据请求，进行处理，将处理结果填充到 rsp 中
      HttpProcess(req, rsp);
      // 将处理结果响应给客户端
      rsp.NormalProcess(sock);
      // 当前采用短连接，直接处理完毕后关闭套接字
      sock.Close();
      return;
    }

    static bool HttpProcess(HttpRequest &req, HttpResponse &rsp) {
      return true;
    }

  private:
    TcpSocket _lst_sock;
    ThreadPool _pool;
    Epoll _epoll;
};
