#ifndef __LIBP2P_URIPARSE_H_
#define __LIBP2P_URIPARSE_H_

#include <stdint.h>

#include <string>
#include <vector>

// Low-level interface

enum uriComponentTy {
  uriCtSchema = 0,
  uriCtUserInfo,
  uriCtHostIPv4,
  uriCtHostIPv6,
  uriCtHostDNS,
  uriCtPort,
  uriCtPath,
  uriCtQuery,
  uriCtFragment
};


struct URIComponent {
  int type;
  union {
    struct {
      const char *data;
      size_t size;
    } raw;
    int i32;
  };
};

typedef void uriParseCb(URIComponent *component, void *arg);

int uriParse(const char *uri, uriParseCb callback, void *arg);

// High-level interface

struct URI {
public:
  enum {
    HostTypeNone = 0,
    HostTypeIPv4,
    HostTypeIPv6,
    HostTypeDNS
  };
  
  std::string schema;
  std::string userInfo;
  
  int hostType;
  union {
    uint32_t ipv4;
    uint16_t ipv6[8];
  };
  
  std::string domain;  
  int port;
  
  std::string path;
  std::string query;
  std::string fragment;
  
public:
  void build(std::string &out);
protected:
    std::string data;
};

int uriParse(const char *uri, URI *data);

#endif //__LIBP2P_URIPARSE_H_