#include <unordered_map>
#include "tcpsocket.hpp"
class HttpRequest {
  public:
    std::string _method;
    std::string _path;
    std::unordered_map<std::string, std::string> _param;
    std::unordered_map<std::string, std::string> _headers;
    std::string _body;
  private:
    bool RecvHeader(TcpSocket &sock,std::string header) {
      //1.探测性接收大量数据
      while(1) {
        std::string tmp;
        if(sock.RecvPeek(tmp) == false) {
          return false;
        }
        //2.判断是否包含整个头部\r\n\r\n
        size_t pos;
        pos = tmp.find_first_of("\r\n\r\n");
        //3.判断当前接收的数据长度
        if(pos == std::string::npos && tmp.size() == 8192) {
          return false;
        }
        else if(pos != std::string::npos) {
        //4.若包含整个头部，则直接获取头部
          size_t header_length = pos+4;
          sock.Recv(header,header_length);
          return true;
        }
      }
    }
    bool FirstLineParse(std::string &line);
    bool PathIsLegal();
  public:
    int RequestParse(TcpSocket &sock) {
      //1.接收http头部
      std::string header;
      if(RecvHeader(sock,header) == false) {
        return 400;
      }
      //2.对整个头部进行按照\r\n进行分割得到一个list
      
      //3.list[0]--首行，进行首行解析
      //4.头部分割解析
      //5.请求信息校验
    }
    
};

class HttpResponse {
  public:
    int _status;
  public:
    bool ErrorProcess(TcpSocket &sock) {
      return true;
    }
    bool NormalProcess(TcpSocket &sock) {
      return true;
    }
};
