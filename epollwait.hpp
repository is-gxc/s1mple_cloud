#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sys/epoll.h>
#include "tcpsocket.hpp"
using namespace std;

#define MAX_EPOLL 1024
class Epoll 
{
    public:
        Epoll()
            :_epfd(-1)
        {}
    public:
        bool EpollInit()
        {
            //创建一个eventpoll结构, 包括红黑树和链表
            _epfd = epoll_create(MAX_EPOLL);
            if(_epfd < 0)
            {
                cerr << "epoll_create error" << endl;
                return false;
            }
            return true;
        }

        bool EpollAdd(TcpSocket& sock)
        {
            //向创建的eventpoll结构中的红黑树中添加节点epoll_event
            int fd = sock.GetFd();
            epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev); 
            if(ret < 0)
            {
                cerr << "add error" << endl;
                return false;
            }
            return true;
        }
        bool EpollDel(TcpSocket& sock)
        {
            int fd = sock.GetFd();
            int ret = epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
            if(ret < 0)
            {
                cerr << "delete error" << endl;
                return false;
            }
            return true;
        }
        bool EpollWait(vector<TcpSocket>& list, int timeout = 3000)
        {
            epoll_event events[MAX_EPOLL];
            int nfds = epoll_wait(_epfd, events, MAX_EPOLL, timeout);
            if(nfds < 0)
            {
                cerr << "wait error" << endl;
                return false;
            }
            else if(nfds == 0)
            {
                //cout << "wait timeout" << endl;
                return false;
            }
            for(int i = 0;i < nfds;i++)
            {
                int fd = events[i].data.fd;
                TcpSocket sock;
                sock.SetFd(fd);
                list.push_back(sock);
            }
            return true;
        }
    private:
        int _epfd;
};

