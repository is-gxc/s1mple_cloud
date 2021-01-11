#pragma once
#include "tcpsocket.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
using namespace std;
class HttpRequest
{
    public:
        //请求方法, 请求路径, 查询字符串, 请求头部信息, 请求正文
        string _method;
        string _path;
        unordered_map<string, string> _param;
        unordered_map<string, string> _headers;
        string _body;
    public:
        bool RecvHeader(TcpSocket& sock, std::string& header)
        {
            while(1)
            {
                //接收头部
                //1.探测接收大量数据
                string tmp;
                if(sock.SocketRecvPeek(tmp) == false)
                {
                    return false;
                }
                //2.判断是否包含整个头部\r\n
                size_t pos;
                pos = tmp.find("\r\n\r\n", 0);
                //3.判断当前接收的数据长度
                //如果没找到, 并且接收的数据为8192字节, 返回错误
                if(pos == string::npos && tmp.size() == 8192)
                {
                    return false;
                }
                else if(pos != string::npos)
                {
                    //4.若包含正个头部, 则直接获取头部
                    //头部长度就是头部+\r\n\r\n
                    size_t header_len = pos;
                    sock.SocketRecv(header, header_len);
                    sock.SocketRecv(tmp, 4);
                    return true;
                }
            }
            return true;
        }
        bool FirstLineParse(string &line)
        {
            //解析首行 
            //GET / HTTP/1.1
            //1.将首行按空格进行解析, 放到vector中 
            vector<string> line_list;
            boost::split(line_list, line, boost::is_any_of(" "), boost::token_compress_on);
            int n = line_list.size();
            if(n != 3)
            {
                cerr << "parse error" << endl;
                return false;
            }
            //请求方法是第一个
            _method = line_list[0];
            //看看有没有查询字符串, 如果没有直接返回
            size_t pos = line_list[1].find("?", 0);
            if(pos == string::npos)
            {
                _path = line_list[1];
            }
            else 
            {
                //如果有, 将查询字符串找出来
                _path = line_list[1].substr(0, pos);
                string query_string = line_list[1].substr(pos + 1);
                //query_string: k=val&kay=val
                //对查询字符串进行解析
                //key=val的形式, 先用&进行split, 把每一个分隔开
                vector<string> param_list;
                boost::split(param_list, query_string, boost::is_any_of("&"), boost::token_compress_on);
                for(auto i : param_list)
                {
                    //将每一个键值对进行解析
                    size_t param_pos = -1;
                    param_pos = i.find("=");
                    if(param_pos == string::npos)
                    {
                        cerr << "parse param error" << endl;
                        return false;
                    }
                    //将=前后解析出来, 放到vector中, key为下标
                    string key = i.substr(0, param_pos);
                    string val = i.substr(param_pos + 1);
                    _param[key] = val;
                }
            }
            return true;
        }
        bool PathIsLegal();//请求资源是否合法
    public:
        int RequestParse(TcpSocket &sock)
        {
            //1.接收HTTP头部
            string header;
            if(RecvHeader(sock, header) == false)
            {
                return 400;
            }
            //2.对头部进行分割(\r\n),得到list,使用boost库split, token_compress_on(去掉空行)
            vector<string> header_list;
            boost::split(header_list, header, boost::is_any_of("\r\n"), boost::token_compress_on);
             //3.list[0]-首行,进行首行解析
            if(FirstLineParse(header_list[0]) == false)
            {
                return false;
            }
            //4.头部解析
            //key: \r\n
            size_t pos = 0;
            for(size_t i = 1; i < header_list.size(); i++)
            {
                pos = header_list[i].find(": ");
                if(pos == string::npos)
                {
                    cerr << "header parse error" << endl;
                    return false;
                }
                string key = header_list[i].substr(0, pos);
                string val = header_list[i].substr(pos + 2);
                _headers[key] = val;
            }
            //5.请求信息校验
            //6.接收正文
            auto it = _headers.find("Content-Length");
            if(it != _headers.end())
            {
                stringstream tmp;
                tmp << it->second;
                int64_t file_len;
                tmp >> file_len;
                sock.SocketRecv(_body, file_len);
            }
            return 200;
        }
};
class HttpResponse
{
    public:
        int _status;
        string _body;
        unordered_map<string, string> _headers;
    private:
        string GetDesc()
        {
            //获取描述信息
            switch(_status)
            {
                case 400: return "Bad Resquse";
                case 404: return "Not Found";
                case 200: return "ok";
            }
            return "UnKnow";
        }
    public:
        bool SetHeader(const string& key, const string& val)
        {
            _headers[key] = val;
            return true;
        }
        bool ErrorProcess(TcpSocket& sock)
        {
            return true;
        }

        bool NormalProcess(TcpSocket& sock)
        {
            //组织数据
            //首行
            string line;
            string header;
            std::stringstream tmp;

            tmp << "HTTP/1.1" << " " << _status << " " << GetDesc();
            tmp << "\r\n";
            //如果没找到正文长度, 就自己添加
            if(_headers.find("Content-Length") == _headers.end())
            {
                string len = to_string(_body.size());
                _headers["Content-Length"] = len;
            }
            for(auto i : _headers)
            {
                tmp << i.first << ": " << i.second << "\r\n";
            }
            tmp << "\r\n";
            sock.SocketSend(tmp.str());
            sock.SocketSend(_body);
            return true;
        }
};
