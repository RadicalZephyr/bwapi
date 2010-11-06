#include "Connect.h"
#include "Common.h"
#include "Threads.h"

namespace LUDP
{
  char gszThisIP[16];

  SOCKET   gsSend;
  SOCKET   gsRecv;
  SOCKADDR gaddrSend;
  SOCKADDR gaddrRecv;

  SOCKET   gsBroadcast;
  SOCKADDR gaddrBroadcast;
  SOCKADDR gaddrBCFrom;

  int SendData(SOCKET s, const char *buf, int len, const SOCKADDR *to)
  {
    int rval = sendto(s, buf, len, 0, to, sizeof(SOCKADDR));
    ++gdwSendCalls;
    gdwSendBytes += len;

    if ( rval == SOCKET_ERROR )
    {
      DWORD dwErr = WSAGetLastError();
      char source[16];

      SOCKADDR name;
      int namelen = sizeof(SOCKADDR);
      getsockname(gsSend, &name, &namelen);

      SStrCopy(source, ip(name.sa_data), 16);
      Error(dwErr, "Unable to send data: %s->%s", source, ip(to->sa_data));
    }
    return rval;
  }

  bool InitializeSockets()
  {
    // do initialization stuff
    WSADATA wsaData;
    DWORD dwErr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if ( dwErr != ERROR_SUCCESS )
    {
      Error(dwErr, "WSAStartup failed.");
      return false;
    }

    // create sockets
    gsSend      = MakeUDPSocket();
    gsRecv      = MakeUDPSocket();
    gsBroadcast = MakeUDPSocket();

    InitAddr(&gaddrRecv,      gdwProcId,         6112);
    InitAddr(&gaddrSend,      gdwProcId,         6112);
    InitAddr(&gaddrBCFrom,    gdwProcId,         6112);
    InitAddr(&gaddrBroadcast, "127.255.255.255", 6112);

    SStrCopy(gszThisIP, ip(gaddrSend.sa_data), 16);

    // bind the sockets
    if ( bind(gsRecv,      &gaddrRecv,   sizeof(SOCKADDR)) == SOCKET_ERROR )
      Error(WSAGetLastError(), "Unable to bind recv socket.");
    if ( bind(gsSend,      &gaddrSend,   sizeof(SOCKADDR)) == SOCKET_ERROR )
      Error(WSAGetLastError(), "Unable to bind send socket.");
    if ( bind(gsBroadcast, &gaddrBCFrom, sizeof(SOCKADDR)) == SOCKET_ERROR )
      Error(WSAGetLastError(), "Unable to bind broadcast socket.");

    // begin recv threads here
    HANDLE hRecvThread = CreateThread(NULL, 0, &RecvThread, NULL, 0, NULL);
    if ( !hRecvThread )
      Error(ERROR_INVALID_HANDLE, "Unable to create the recv thread.");
    SetThreadPriority(hRecvThread, 1);
    return true;
  }

  void DestroySockets()
  {
    // do cleanup stuff
    if ( gsSend )
      closesocket(gsSend);
    if ( gsRecv )
      closesocket(gsRecv);
    if ( gsBroadcast )
      closesocket(gsBroadcast);
    WSACleanup();
  }

  SOCKET MakeUDPSocket()
  {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if ( s == INVALID_SOCKET )
    {
      Error(WSAGetLastError(), "A socket could not be created.");
      return NULL;
    }

    DWORD dwTrue  = 1;
    if ( setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&dwTrue, sizeof(DWORD)) == SOCKET_ERROR )
      Error(WSAGetLastError(), "The socket option SO_REUSEADDR could not be set.");
    if ( setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&dwTrue, sizeof(DWORD)) == SOCKET_ERROR )
      Error(WSAGetLastError(), "The socket option SO_BROADCAST could not be set.");
    return s;
  }

  SOCKADDR *InitAddr(SOCKADDR *addr, const char *ip, WORD wPort)
  {
    sockaddr_in *_addr = (sockaddr_in*)addr;
    memset(addr, 0, sizeof(SOCKADDR));
    _addr->sin_family           = AF_INET;
    _addr->sin_port             = htons(wPort);
    _addr->sin_addr.S_un.S_addr = inet_addr(ip);
    return addr;
  }

  SOCKADDR *InitAddr(SOCKADDR *addr, DWORD dwSeed, WORD wPort)
  {
    sockaddr_in *_addr = (sockaddr_in*)addr;
    memset(addr, 0, sizeof(SOCKADDR));
    _addr->sin_family                 = AF_INET;
    _addr->sin_port                   = htons(wPort);
    _addr->sin_addr.S_un.S_un_b.s_b1  = 127;
    _addr->sin_addr.S_un.S_un_b.s_b2  = (dwSeed >> 16) & 0xFF;
    _addr->sin_addr.S_un.S_un_b.s_b3  = (dwSeed >> 8)  & 0xFF;
    _addr->sin_addr.S_un.S_un_b.s_b4  = dwSeed & 0xFF;
    if ( _addr->sin_addr.S_un.S_un_b.s_b4 == 0 )
    {
      _addr->sin_addr.S_un.S_un_b.s_b4++;
      _addr->sin_addr.S_un.S_un_b.s_b2 |= 0x40;
    }
    else if ( _addr->sin_addr.S_un.S_un_b.s_b4 == 255 )
    {
      _addr->sin_addr.S_un.S_un_b.s_b4++;
      _addr->sin_addr.S_un.S_un_b.s_b2 |= 0x80;
    }
    return addr;
  }

};
