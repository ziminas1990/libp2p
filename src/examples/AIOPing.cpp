#include "asyncio/asyncio.h"
#include "asyncio/dynamicBuffer.h"
#include "asyncio/timer.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

#if !defined(OS_WINDOWS)
#include <netdb.h>
#endif

static option longOpts[] = {
  {"interval", required_argument, 0, 'i'},
  {"count", required_argument, 0, 'c'},
  {"help", no_argument, 0, 0},
  {0, 0, 0, 0}
};

static const char shortOpts[] = "i:c:";
static const char *gTarget = 0;
static double gInterval = 1.0;
static unsigned gCount = 4;


static const uint8_t ICMP_ECHO = 8;
static const uint8_t ICMP_ECHOREPLY = 0;


#pragma pack(push, 1)
  struct ip {
    uint8_t ip_verlen;
    uint8_t ip_tos;
    uint16_t ip_len;
    uint16_t ip_id;
    uint16_t ip_fragoff;
    uint8_t ip_ttl;
    uint8_t ip_proto;
    uint16_t ip_chksum;
    uint32_t ip_src_addr;
    uint32_t ip_dst_addr;
  };

  struct icmp {
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t icmp_cksum;
    uint32_t icmp_id;
    uint16_t icmp_seq;
  };
#pragma pack(pop)


struct ICMPClientData {
  aioObject *rawSocket;
  HostAddress remoteAddress;
  icmp data;
  uint32_t id;
  dynamicBuffer buffer;
  std::map<unsigned, timeMark> times;
};

uint16_t InternetChksum(uint16_t *lpwIcmpData, uint16_t wDataLength)
{
  uint32_t lSum;
  uint16_t wOddByte;
  uint16_t wAnswer;

  lSum = 0L;

  while (wDataLength > 1) {
    lSum += *lpwIcmpData++;
    wDataLength -= 2;
  }

  if (wDataLength == 1)  {
    wOddByte = 0;
    *((uint8_t*) &wOddByte) = *(uint8_t*)lpwIcmpData;
    lSum += wOddByte;
  }

  lSum = (lSum >> 16) + (lSum & 0xffff);
  lSum += (lSum >> 16);
  wAnswer = (uint16_t)~lSum;
  return(wAnswer);
}


void printHelpMessage(const char *appName)
{
  printf("Usage: %s help:\n"
    "%s <options> address\n"
    "General options:\n"
    "  --count or -c packets count\n"
    "  --interval or -i interval between packets sending\n",
    appName, appName);
}


void readCb(aioInfo *info)
{
  ICMPClientData *client = (ICMPClientData*)info->arg;
  if (info->status == aosSuccess &&
      info->bytesTransferred >= (sizeof(ip) + sizeof(icmp))) {
    dynamicBufferSeek(&client->buffer, SeekSet, 0);
    uint8_t *ptr = (uint8_t*)dynamicBufferPtr(&client->buffer);
    icmp *receivedIcmp = (icmp*)(ptr + sizeof(ip));

    if (receivedIcmp->icmp_type == ICMP_ECHOREPLY) {
      std::map<unsigned, timeMark>::iterator F =
        client->times.find(receivedIcmp->icmp_id);
      if (F != client->times.end()) {
        double diff = (double)usDiff(F->second, getTimeMark());
        fprintf(stdout,
                " * [%u] response from %s %0.4lgms\n",
                (unsigned)receivedIcmp->icmp_id,
                gTarget,
                diff / 1000.0);
        client->times.erase(F);
      }
    }
  }
  
  asyncReadMsg(client->rawSocket, &client->buffer, 0, readCb, client);
}

void pingTimerCb(aioInfo *info)
{
  ICMPClientData *clientData = (ICMPClientData*)info->arg;
  clientData->id++;
  clientData->data.icmp_id = clientData->id;
  clientData->data.icmp_cksum = 0;
  clientData->data.icmp_cksum =
    InternetChksum((uint16_t*)&clientData->data, sizeof(icmp));
    
  asyncWriteMsg(clientData->rawSocket,
                &clientData->remoteAddress,
                &clientData->data, sizeof(icmp),
                afNone, 1000000, 0, 0);
  clientData->times[clientData->id] = getTimeMark();
}


void printTimerCb(aioInfo *info)
{
  std::vector<uint32_t> forDelete;
  ICMPClientData *clientData = (ICMPClientData*)info->arg;
  for (std::map<unsigned, timeMark>::iterator I = clientData->times.begin(),
       IE = clientData->times.end(); I != IE; ++I) {
    uint64_t diff = usDiff(I->second, getTimeMark());
    if (diff > 1000000) {
      fprintf(stdout, " * [%u] timeout\n", (unsigned)I->first);
      forDelete.push_back(I->first);
    }
  }

  for (size_t i = 0; i < forDelete.size(); i++)
    clientData->times.erase(forDelete[i]);
}


int main(int argc, char **argv)
{
  int res, index = 0;
  while ((res = getopt_long(argc, argv, shortOpts, longOpts, &index)) != -1) {
    switch(res) {
      case 0 :
        if (strcmp(longOpts[index].name, "help") == 0) {
          printHelpMessage("modstress");
          return 0;
        }
        break;
      case 'c' :
        gCount = atoi(optarg);
        break;
      case 'i' :
        gInterval = atof(optarg);
        break;
      case ':' :
        fprintf(stderr, "Error: option %s missing argument\n",
                longOpts[index].name);
        break;
      case '?' :
        fprintf(stderr, "Error: invalid option %s\n", argv[optind-1]);
        break;
      default :
        break;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "You must specify target address, see help\n");
    return 1;
  }

  gTarget = argv[optind];
  initializeSocketSubsystem();
  HostAddress localAddress;  

  hostent *host = gethostbyname(gTarget);
  if (!host) {
    fprintf(stderr,
            " * cannot retrieve address of %s (gethostbyname failed)\n",
            argv[optind]);
    return 1;
  }

  u_long addr = host->h_addr ? *(u_long*)host->h_addr : 0;
  if (!addr) {
    fprintf(stderr,
            " * cannot retrieve address of %s (gethostbyname returns 0)\n",
            argv[optind]);
    return 1;
  }

  localAddress.family = AF_INET;
  localAddress.ipv4 = INADDR_ANY;
  localAddress.port = 0;
  socketTy S = socketCreate(AF_INET, SOCK_RAW, IPPROTO_ICMP, 1);
  socketReuseAddr(S);
  if (socketBind(S, &localAddress) != 0) {
    fprintf(stderr, " * bind error\n");
    exit(1);
  }

  ICMPClientData client;
  client.id = 0;
  client.remoteAddress.family = AF_INET;
  client.remoteAddress.ipv4 = addr;

  client.data.icmp_type = ICMP_ECHO;
  client.data.icmp_code = 0;
  client.data.icmp_seq = 0;

  asyncBase *base = createAsyncBase(amOSDefault);
  asyncOp *pingTimer = newUserEvent(base, pingTimerCb, &client);
  asyncOp *printTimer = newUserEvent(base, printTimerCb, &client);
  client.rawSocket = newSocketIo(base, S);

  dynamicBufferInit(&client.buffer, 1024);
  asyncReadMsg(client.rawSocket, &client.buffer, 0, readCb, &client);
  userEventStartTimer(printTimer, 100000, -1);
  userEventStartTimer(pingTimer, (uint64_t)(gInterval*1000000), -1);
  asyncLoop(base);
  return 0;
}
