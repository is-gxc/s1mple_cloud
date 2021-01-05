#pragma once
#include <iostream>
#include <string>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class TcpSocket {
  public:
    TcpSocket():_sockfd(-1){

    }

    int GetFd() {
      return _sockfd;
    }

    void SetFd(int fd) {
      _sockfd = fd;
    }

    void SetNonBlock() {
      int flag = fcntl(_sockfd, F_GETFL, 0);
      fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }

    bool SocketInit(int port) {
      _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if(_sockfd < 0) {
        std::cerr << "socket error\n";
        return false;
      }
      int opt = 1;
      setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
      struct sockaddr_in addr;
      addr.sin_addr.s_addr = inet_addr("0.0.0.0");
      addr.sin_port = htons(port);
      addr.sin_family = AF_INET;
      socklen_t len = sizeof(addr);
      int ret = bind(_sockfd, (struct sockaddr*)&addr, len);
      if(ret < 0) {
        std::cerr << "bind error\n";
        close(_sockfd);
        return false;
      }
      ret = listen(_sockfd, 10);
      if(ret < 0) {
        std::cerr << "listen error\n";
        close(_sockfd);
        return false;
      }
      return true;
    }

    bool Accept(TcpSocket &sock) {
      int fd;
      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);
      fd = accept(_sockfd, (struct sockaddr*)&addr, &len);
      if(fd < 0) {
        std::cerr << "accept error\n";
        return false;
      }
      sock.SetFd(fd);
      sock.SetNonBlock();
      return true;
    }

    bool RecvPeek(std::string &buf) {
      char tmp[8192] = {0};
      int ret = recv(_sockfd, tmp, 8192, MSG_PEEK);
      if(ret <= 0) {
        if(errno == EAGAIN) {
          return true;
        }
        std::cerr << "recv error\n";
        return false;
      }
      buf.assign(tmp, ret);
      return true;
    }

    bool Recv(std::string &buf, int len) {
      buf.resize(len);
      int rlen = 0,ret;
      while(rlen < len) {
        ret = recv(_sockfd, &buf[0]+rlen, len-rlen, 0);
        if(ret <= 0) {
          if(errno == EAGAIN) {
            usleep(1000);
            continue;
          }
          return false;
        }
        rlen += ret;
      }
      return true;
    }

    bool Close() {
      close(_sockfd);
    }

  private:
    int _sockfd;
};
