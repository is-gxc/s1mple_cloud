#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>

class Boundary {
  public:
    int64_t _start_addr;
    int64_t _data_len;
    std::string _name;
};

bool GetHeader(const std::string &key, std::string &val) {

  std::string body;
  char *ptr = getenv(key.c_str());
  if(ptr == NULL) {
    return false;
  }
  val = ptr;
  return true;
}

bool BoundaryParse(std::string &body, std::vector<Boundary> &list) {
  std::string cont_b = "boundary=";
  std::string tmp;
  if(GetHeader("Content-Type", tmp) == false) {
    return false;
  }
  size_t pos = tmp.find(cont_b);
  if(pos == std::string::npos) {
    return false;
  }
  std::string boundary = tmp.substr(pos+cont_b.size());
  std::string f_boundary = dash + boundary + craf;
  std::string m_boundary = craf +dash +boundary;
  std::string dash = "--";
  std::string craf = "\r\n";
  std::string tail = "\r\n\r\n";

  size_t pos, next_pos;
  pos = body.find(f_boundary);
  //如果当前f_boundary的位置不是起始位置
  if(pos != 0) {
    std::cerr << "first boundary error\n";
    return false;
  }
  //指向第一块头部起始位置
  next_pos = pos + f_boundary.size();
  while(pos < body.size()) {
    //找寻头部结尾
    pos = body.find(tail, next_pos);
    if(pos == std::string::npos) {
      return false;
    }
    std::string header = body.substr(pos, next_pos - pos);
    //解析头部
    //数据的起始地址
    next_pos = pos = tail.size();
    //找\r\n--boundary 
    pos = body.find(m_boundary, next_pos);
    if(pos == std::string::npos) {
      return false;
    }
    int64_t offset = next_pos; //下一个boundary的起始地址减去数据的起始地址
    int64_t length = pos - next_pos;
    next_pos = pos + m_boundary.size();
    pos = body.find(craf, next_pos);
    if(pos == std::string::npos) {
      return false;
    }
    //pos指向这个下一个m_boundary的头部起始地址
    //若没有m_boundary了，则指向数据的结尾  pos==body.size() 退出循环
    pos += craf.size();  
  }
  return true;
}

int main(int argc, char *argv[], char *env[]) {
  for(int i = 0; env[i] != NULL; i++) {
    std::cerr << "env[i]====[" << env[i] << "]\n";
  }
  std::string body;
  char *cont_len = getenv("Content-Length");
  if(cont_len != NULL) {
    std::stringstream tmp;
    tmp << cont_len;
    int64_t fsize;
    tmp >> fsize;
    
    body.resize(fsize);
    int rlen = 0;
    while(rlen < fsize) {
      int ret = read(0, &body[0] + rlen, fsize - rlen);
      if(ret <= 0) {
        exit(-1);
      } 
      rlen += ret;
    }
    std::cerr << "body:[" << body << "]\n";
  }
  return 0;
}
