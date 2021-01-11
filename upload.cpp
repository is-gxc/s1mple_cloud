#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <vector>
using namespace std;

#define WWW_ROOT "./www/"

class Boundary
{
    public:
        int64_t _start_addr;
        int64_t _data_len;
        string _name;
        string _filename;
};

bool GetHeader(const string& key, string& val)
{
    string body;
    char* ptr = getenv(key.c_str());
    if(ptr == NULL)
    {
        return false;
    }
    val = ptr;
    return true;
}

bool HeaderParse(string& header, Boundary& file)
{
    vector<string> list;
    boost::split(list, header, boost::is_any_of("\r\n"), boost::token_compress_on);
    for(size_t i = 0; i < list.size(); i++)
    {
        string sep = ": ";
        size_t pos = list[i].find(sep);
        if(pos == string::npos)
        {
            cerr << "find :  error" << endl;
            return false;
        }
        string key = list[i].substr(0, pos);
        string val = list[i].substr(pos + sep.size());
        if(key != "Content-Disposition")
        {
            cerr << "can not find Dispostion" << endl;
            continue;
        }
        string name_field = "fileupload";
        string filename_sep = "filename=\"";

        pos = val.find(name_field);
        if(pos == string::npos)
        {
            cerr << "have no fileupload area" << endl;
            continue; 
        }
        pos = val.find(filename_sep);
        if(pos == string::npos)
        {
            cerr << "have no filename" << endl;
            return false;
        }
        pos += filename_sep.size();
        size_t next_pos = val.find("\"", pos);
        if(next_pos == string::npos)
        {
            cerr << "have no \"" << endl;
            return false;
        }
        file._filename = val.substr(pos, next_pos - pos);
        file._name = "fileupload";
    }
    return true;
}

// Boundary解析
bool BoundaryParse(string& body, vector<Boundary>& list)
{
    string cont_b = "boundary=";
    string tmp; 
    if(GetHeader("Content-Type", tmp) == false)
    {
        return false;
    }
    size_t pos = tmp.find(cont_b);
    if(pos == string::npos)
    {
        return false;
    }

    string boundary = tmp.substr(pos + cont_b.size());
    string dash = "--";
    string craf = "\r\n";
    string tail = "\r\n\r\n";
    string f_boundary = dash + boundary + craf;
    string m_boundary = craf + dash + boundary;
    size_t nex_pos;
    //找第一个boundary
    pos = body.find(f_boundary);
    if(pos != 0)
    {
        //如果f_boundary的位置不是起始位置, 则错误
        cerr << "first boundary error" << endl;
        return false;
    }
    //找第一块头部起始位置
    pos += f_boundary.size();
    while(pos < body.size())
    {
        //找寻头部结尾
        nex_pos = body.find(tail, pos);
        if(nex_pos == string::npos)
        {
            return false;
        }
        //获取头部
        string header = body.substr(pos, nex_pos - pos);

        //nex_pos指向数据的起始地址
        pos = nex_pos + tail.size();
        //找\r\n--boundary,数据的结束位置,  如果没有则格式错误
        //next->\r\n--
        nex_pos = body.find(m_boundary, pos);
        if(nex_pos == string::npos)
        {
            return false;
        }
        int64_t offset = pos;
        //下一个boundary的起始地址 - 数据的起始地址, 数据长度
        int64_t len = nex_pos - pos;
        nex_pos += m_boundary.size();//指向\r\n
        pos = body.find(craf, nex_pos);
        if(pos == string::npos)
        {
            return false;
        } 
        //pos指向下一个m_boundary的头部起始地址
        //若没有m_boundary了, 则指向数据结尾, pos = body.size()
        pos += craf.size(); 
        Boundary file;
        file._data_len = len;
        file._start_addr = offset;
        //解析头部
        if(HeaderParse(header, file) == false)
        {
            cerr << "header parse error" << endl;
            return false;
        }
        list.push_back(file);
    }
    cerr << "parse boundary over" << endl;
    return true;
}

bool StorageFile(string& body,vector<Boundary>& list)
{
    //存储文件
    for(size_t i = 0; i < list.size(); i++)
    {
        if(list[i]._name != "fileupload")
        {
            continue;
        }
        string realpath = WWW_ROOT + list[i]._filename;
        ofstream file(realpath);
        if(!file.is_open())
        {
            cerr << "open file" << realpath << "failed" << endl;
            return false;
        }
        file.write(&body[list[i]._start_addr], list[i]._data_len);
        if(!file.good())
        {
            cerr << "write file error" << endl;
            return false;
        }
        file.close();
    }
    return true;
}

int main(int argc, char* argv[], char* env[])
{
    string body;
    string error = "<html><h1>Failed!!!</h1></html>";
    string suc = "<html><h1>success!!!</h1></html>";
    char* con_len = getenv("Content-Length");
    if(con_len != NULL)
    {
        stringstream tmp;
        tmp << con_len;
        int64_t filesize;
        tmp >> filesize;

        body.resize(filesize);
        int rlen = 0;
        int ret;
        while(rlen < filesize)
        {
            ret = read(0, &body[0] + rlen, filesize - rlen);
            if(ret <= 0)
            {
                exit(-1);
            }
            rlen += ret;
        }
        vector<Boundary> list;
        bool cur = BoundaryParse(body, list);
        if(cur == false)
        {
            cerr << "BoundaryParse error" << endl;
            cout << error;
            return -1;
        }
        for(auto i : list)
        {
            cerr << "name:[" << i._name << "]" << endl;
            cerr << "filename:[" << i._filename << "]" << endl;
        }
        cur = StorageFile(body, list);
        if(cur == false)
        {
            cerr << "StorageFile error" << endl;
            cout << error;
            return -1;
        }
        cout << suc;
        return 0;
    } 
    cout << error;
    return 0;
}
