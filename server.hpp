/*
 * 实现server类，完成服务端的整体结构流程
 */
#include <boost/filesystem.hpp>
#include "tcpsocket.hpp"
#include "epollwait.hpp"
#include "threadpool.hpp"
#include "http.hpp"

#define WWW_ROOT "./www"

class Server {
  public:
    bool Start(int port) {
      bool ret;
      ret = _lst_sock.SocketInit(port);
      if(ret == false) {
        return false;
      }
      /*
      ret = _epoll.Init();
      if(ret == false) {
        return false;
      }*/
      ret = _pool.PoolInit();
      if(ret == false) {
        return false;
      }/*
      _epoll.Add(_lst_sock);*/
      while(1) {
            TcpSocket cli_sock;
            ret = _lst_sock.Accept(cli_sock);
            if(ret == false) {
              continue;
            }
            cli_sock.SetNonBlock();
            ThreadTask tt(cli_sock.GetFd(), ThreadHandler);
            _pool.TaskPush(tt);
        /*    
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
        }*/
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
      std::cout << "-------------------------\n";
      // 根据请求，进行处理，将处理结果填充到 rsp 中
      HttpProcess(req, rsp);
      // 将处理结果响应给客户端
      rsp.NormalProcess(sock);
      // 当前采用短连接，直接处理完毕后关闭套接字
      sock.Close();
      return;
    }

    static bool HttpProcess(HttpRequest &req, HttpResponse &rsp) {
      //若请求是一个POST请求--则多进程CGI进行处理
      //若请求是一个GET请求--但是查询字符串不为空
      //否则，若请求是GET，并且查询字符串为空
      // 若请求是一个目录
      // 若请求是一个文件
      std::string realpath = WWW_ROOT + req._path;
      if(!boost::filesystem::exists(realpath)) {
        std::cerr << "realpath:[" << realpath << "]\n";
        rsp._status = 404;
        return false;
      }
      if((req._method == "GET" && req._param.size() != 0) || req._method == "POST") {
        //对于当前来说，则是一个文件上传请求
      }
      else {
        //则是一个基本的文件下载/目录列表请求
        if(boost::filesystem::is_directory(realpath)) {
          std::cerr << "into is_directory\n";
          //查看目录列表请求
          ListShow(realpath, rsp._body);
          rsp.SetHeader("Content-Type", "text/html");
        }
        else {
          //普通的文件下载请求
        }
      }
      rsp._status = 200;
      return true;
    }

    static bool ListShow(std::string &path, std::string &body) {
      std::stringstream tmp;
      tmp << "<html><head><style>";
      tmp << "* {margin : 0;}";
      tmp << ".main-window {height : 100%;width : 80%;margin : 0 auto;}";
      tmp << ".upload {position : relative;height : 20%;width : 100%;background-color : #33c0b9;}";
      tmp << ".listshow {position : relative;height : 80%;width : 100%;background : #6fcad6;}";
      tmp << "</style></head>";
      tmp << "<body><div class='main-window'>";
      tmp << "<div class='upload'></div><hr />";
      tmp << "<div class='listshow'><ol>";
      //***************组织每个文件节点信息
      boost::filesystem::directory_iterator begin(path);
      boost::filesystem::directory_iterator end;
      for(;begin != end; ++begin) {
        std::string pathname = begin->path().string();
        std::string name = begin->path().filename().string();
        int64_t ssize = boost::filesystem::file_size(name);
        int64_t mtime = boost::filesystem::last_write_time(name);
        tmp << "<li><strong><a href='#'>";
        tmp << name;
        tmp<< "</a><br /></strong>";
        tmp << "<small>modified";
        tmp << mtime;
        tmp << "<br /> filetype: application-octostream ";
        tmp << ssize/1024 << "kbytes";
        tmp << "</small></li>";
      }
      //***************组织每个文件节点信息
      tmp << "</ol></div><hr /></div></body></head></html>";
      body = tmp.str();
      return true;
    }

  private:
    TcpSocket _lst_sock;
    ThreadPool _pool;
    Epoll _epoll;
};
