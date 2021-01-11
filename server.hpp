#pragma once
#include <vector>
#include "tcpsocket.hpp"
#include "epollwait.hpp"
#include "threadpool.hpp"
#include "http.hpp"
#include <boost/filesystem.hpp>
#include <stdlib.h>
#include <fstream>
using namespace std;

#define WWW_ROOT "./www"

class Server
{
    public:
        bool Start(int port)
        {
            bool ret;
            //init中实现创建套接字, 绑定地址, 开始监听
            ret = _lst_sock.SocketInit(port);
            if(ret == false)
            {
                return false;
            }
            //创建并初始化epoll, 套接字未就绪的话就不用一直等待, 让操作系统来监听
            //如果客户端一直不发送消息就会占用线程池, 所以我们采用事件总线进行监控, 这样不会占用资源
            ret = _epoll.EpollInit();
            if(ret == false)
            {
                return false;
            }
            //pool init
            ret = _pool.PoolInit();
            if(ret == false)
            {
                return false;
            }
            //将监听套接字添加到epoll红黑树中
            _epoll.EpollAdd(_lst_sock);
            while(1)
            {
                //epoll开始监听, 如果有就绪的描述符则添加进链表中, 然后将就绪的描述符全部pushback进list中
                vector<TcpSocket> list;
                ret = _epoll.EpollWait(list);
                if(ret == false)
                {
                    continue;
                }
                for(size_t i = 0;i < list.size();i++)
                {
                    if(list[i].GetFd() == _lst_sock.GetFd())
                    {
                        //如果是监听套接字就获取新连接, 然后将新的描述符添加进红黑树中
                        TcpSocket cli_sock;
                        ret = _lst_sock.SocketAccept(cli_sock);
                        if(ret == false)
                        {
                            return false;
                        }
                        //描述符设置非阻塞
                        cli_sock.SetNonblock();
                        //添加到epoll中
                        _epoll.EpollAdd(cli_sock);
                    }
                    else 
                    {
                        //不是监听套接字则组织一个任务抛进线程池, 设置任务(socket和对应的处理函数), 添加进任务队列
                        //但是这里注意, 一定要在_epoll中Del, epoll中包含每一个sockfd, 要是不进行删除, 会一直触发事件
                        ThreadTask tt(list[i].GetFd(), ThreadHandler);
                        _pool.TaskPush(tt);
                        _epoll.EpollDel(list[i]);
                    }
                }
            }
            _lst_sock.SocketClose();
            return true;
        }

    public:
        static void ThreadHandler(int sockfd)
        {
            TcpSocket sock;
            sock.SetFd(sockfd);
            //请求类, 进行请求解析
            HttpRequest req;
            //回复类, 组织回复数据
            HttpResponse rsp;
            int status = req.RequestParse(sock);
            if(status != 200)
            {
                //则直接响应错误
                rsp._status = status;
                rsp.ErrorProcess(sock);
                //响应完成之后关闭套接字
                sock.SocketClose();
                return;
            }
            //根据req进行处理
            HttpProcess(req, rsp);
            //将处理结果响应给客户端
            rsp.NormalProcess(sock);
            //当前采用短连接, 直接处理完毕后关闭套接字
            sock.SocketClose();
            return;
        }

        static bool HttpProcess(HttpRequest &req, HttpResponse &rsp)
        {
            //如果请求是post请求----应该是CGI处理, 多进程进行处理
            //若请求是一个GET请求---但是若查询字符串不为空, 也是CGI
            //否则, 如果请求时GET, 并且查询字符串为空
            //      若请求的是一个目录: 查看文件列表
            //      若请求的是一个文件: 文件下载
            
            //当前所在路径 + 给的路径 = 真正路径
            string realpath = WWW_ROOT + req._path;
            if(!boost::filesystem::exists(realpath))
            {   
                //如果不存在则将状态码置为404, 客户端错误
                rsp._status = 404;
                cerr << "realpath:[" << realpath << "]" << endl;
                return false;
            }
            if((req._method == "GET" && req._param.size() != 0) || req._method == "POST")
            {
                //文件上传请求
                CGIProcess(req, rsp);
            }
            else 
            {
                //文件下载/文件列表请求
                if(boost::filesystem::is_directory(realpath))
                {
                    //列表展示请求
                    ListShow(req, rsp);
                }
                else 
                {
                    RangeDownload(req, rsp);
                    return true;
                }
            }
            rsp._status = 200;
            return true;
        }

        static int64_t str_to_digit(const string val)
        {
            stringstream tmp;
            tmp << val;
            int64_t dig = 0;
            tmp >> dig;
            return dig;
        }

        static bool RangeDownload(HttpRequest &req, HttpResponse &rsp)
        {
            //Range: bytes=start-[end];
            string realpath = WWW_ROOT + req._path;
            int64_t data_len = boost::filesystem::file_size(realpath);
            int64_t last_mtime = boost::filesystem::last_write_time(realpath);
            string etag = std::to_string(data_len) + std::to_string(last_mtime);
            auto it = req._headers.find("Range");
            if(it == req._headers.end())
            {
                Download(realpath, 0, data_len, rsp._body);
                rsp._status = 200;
            }
            else 
            {
                string range = it->second;
                //Range: bytes = start - end;
                string unit = "bytes=";
                size_t pos = range.find(unit);
                if(pos == string::npos)
                {
                    return false;
                }
                pos += unit.size();
                size_t pos2 = range.find("-", pos);
                if(pos2 == string::npos)
                {
                    return false;
                }
                string start = range.substr(pos, pos2 - pos);
                string end = range.substr(pos2 + 1);
                int64_t dig_start, dig_end;
                dig_start = str_to_digit(start);
                if(end.size() == 0)
                {
                    dig_end = data_len - 1;
                }
                else 
                {
                    dig_end = str_to_digit(end);
                }
                int64_t range_len = dig_end - dig_start + 1;
                Download(realpath, dig_start, range_len, rsp._body);
                stringstream tmp;
                tmp << "bytes " << dig_start << "-" << dig_end << "/" << data_len;
                rsp.SetHeader("Content-Range", tmp.str());
                rsp._status = 206;
            }
            rsp.SetHeader("Content-Type", "application/octet-stream");
            rsp.SetHeader("Accept-Ranges", "bytes");
            rsp.SetHeader("ETag", etag);
            return true;
        }
        
        // 外部程序处理
        static bool CGIProcess(HttpRequest &req, HttpResponse &rsp)
        {
            // 管道用于进程间通信：头部、正文
            // 管道是半双工通信，单向传递，所以需要创建两个管道，一个用于给子进程发送数据，一个用于从子进程取出数据。
            int pipe_in[2], pipe_out[2];
            if(pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
            {
                cerr << "create pipe error" << endl;
                return false;
            }
            int pid = fork();
            if(pid < 0)
            {
                return false;
            }
            else if(pid == 0)
            {
                close(pipe_in[0]);  //子进程写, 父进程读
                close(pipe_out[1]); //子进程读, 父进程写
                dup2(pipe_in[1], 1);//将标准输出重定向到管道的写端, 数据直接写入管道
                dup2(pipe_out[0], 0);
                //通过环境变量设置头部
                // 名称  内容  是否覆盖
                setenv("METHOD", req._method.c_str(), 1);
                setenv("PATH", req._path.c_str(), 1);
                for(auto i : req._headers)
                {
                    setenv(i.first.c_str(), i.second.c_str(), 1);
                }
                string realpath = WWW_ROOT + req._path;
                // 子进程程序替换
                execl(realpath.c_str(), realpath.c_str(), NULL);
                exit(0);
            }
            //父进程中往管道里面写数据
            close(pipe_in[1]);
            close(pipe_out[0]);
            write(pipe_out[1], &req._body[0], req._body.size());
            //父进程等待子进程输出
            while(1)
            {
                char buf[1024] = {0};
                int ret = read(pipe_in[0], buf, 1024);
                if(ret == 0)
                {
                    break;
                }
                buf[ret] = '\0';
                rsp._body += buf;
            }
            close(pipe_in[0]);
            close(pipe_out[1]);
            rsp._status = 200;
            return true;
        }

        static bool Download(string& path, int64_t start, int64_t len, string& body)
        {
            // 根据文件大小扩充空间
            body.resize(len);
            std::ifstream file(path);
            // 打开失败
            if(!file.is_open())
            {
                cerr << "open file failed" << endl;
                return false;
            }
            // 读取数据
            file.seekg(start, std::ios::beg);
            file.read(&body[0], len);
            // 读取失败
            if(!file.good())
            {
                cerr << "read file data failed" << endl;
                return false;
            }
            file.close();
            return true;
        }

        static bool ListShow(HttpRequest &req, HttpResponse &rsp)
        {
            string realpath = WWW_ROOT + req._path;
            string req_path = req._path;
            stringstream tmp;
            tmp << "<html><head><style>";
            tmp << "*{margin : 0;}";
            tmp << ".main-window {height : 100%; width : 80%; margin : 0 auto;}";
            tmp << ".upload{position: relative; height:20%; width: 100%; background-color: #eb757dfb; text-align:center;}";
            tmp << ".listshow {position : relative; height : 80%; width : 100%; background : #49b3dd;}";
            tmp << "</style></head><body>";
            tmp << "<div class='main-window'>";
            tmp << "<div class='upload'>";
            tmp << "<form action='/upload' method='post' enctype='multipart/form-data'>";
            tmp << "<div class='upload-btn'>";
            tmp << "<input type='file' name='fileupload'>";
            tmp << "<input type='submit' name='submit'>";
            tmp << "</div></form>";
            tmp << "</div><hr />";
            tmp << "<div class='listshow'><ol>";
            //组织每个节点信息
            boost::filesystem::directory_iterator begin(realpath);
            boost::filesystem::directory_iterator end;
            for(; begin != end; ++begin)
            {
                //获取带路径的文件名
                string pathname = begin->path().string();
                //对文件名进行截断, 只要文件名.
                string name = begin->path().filename().string();
                //uri
                string uri = req_path + name;
                //最后一次修改时间
                int64_t mtime = boost::filesystem::last_write_time(pathname);
                if(boost::filesystem::is_directory(pathname))
                {
                    //如果是一个目录
                    tmp << "<li><strong><a href='";
                    tmp << uri << "/'>";
                    tmp << name << "/"; 
                    tmp << "</a><br /></strong>";
                    tmp << "<small>modified: ";
                    tmp << mtime;
                    tmp << "<br />filetype: directory ";
                    tmp << "</small></li>";
                }
                else 
                {
                    //文件大小
                    int ssize = boost::filesystem::file_size(pathname);
                    //如果是一个普通文件
                    tmp << "<li><strong><a href='";
                    tmp << uri << "'>";
                    tmp << name; 
                    tmp << "</a><br /></strong>";
                    tmp << "<small>modified: ";
                    tmp << mtime;
                    tmp << "<br /> filetype: application-octstream ";
                    tmp << "<br /> size: ";
                    tmp << ssize / 1024 << "kbytes ";
                    tmp << "</small></li>";
                }
            }
            //组织每个节点信息
            tmp << "</ol></div><hr /></div></body></html>";
            rsp._body = tmp.str();
            rsp._status = 200;
            rsp.SetHeader("Content-Type", "text/html");
            return true;
        }

    private:
        TcpSocket _lst_sock;
        ThreadPool _pool;
        Epoll _epoll;
};
