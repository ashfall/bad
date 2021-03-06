#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <memory>
#include <utility>

#include "domain_socket.hh"
#include "exception.hh"
#include "util.hh"

using namespace std;

/* construct a unix domain socket pair */
pair<UnixDomainSocket, UnixDomainSocket> UnixDomainSocket::NewPair( void )
{
  int pipe[2];
  SystemCall( "socketpair", socketpair( AF_UNIX, SOCK_DGRAM, 0, pipe ) );
  return make_pair( UnixDomainSocket( pipe[0] ), UnixDomainSocket( pipe[1] ) );
}

/* send a file descriptor over a unix domain socket */
void UnixDomainSocket::send_fd( const FileDescriptor & fd )
{
  msghdr message_header;
  zero( message_header );

  unique_ptr<char[]> control_buffer(new char[CMSG_SPACE( sizeof( int ) )] );
  message_header.msg_control = control_buffer.get();
  message_header.msg_controllen = CMSG_SPACE( sizeof( int ) );

  cmsghdr * const control_message = CMSG_FIRSTHDR( &message_header );
  control_message->cmsg_level = SOL_SOCKET;
  control_message->cmsg_type = SCM_RIGHTS;
  control_message->cmsg_len = CMSG_LEN( sizeof( fd.fd_num() ) );
  *reinterpret_cast<int *>( CMSG_DATA( control_message ) ) = fd.fd_num();
  message_header.msg_controllen = control_message->cmsg_len;

  if ( SystemCall( "sendmsg", sendmsg( fd_num(), &message_header, 0 ) ) ) {
    throw runtime_error( "send_fd: sendmsg unexpectedly sent data" );
  }

  register_write();
}

/* receive a file descriptor over a unix domain socket */
FileDescriptor UnixDomainSocket::recv_fd( void )
{
  msghdr message_header;
  zero( message_header );

  unique_ptr<char[]> control_buffer(new char[CMSG_SPACE( sizeof( int ) )] );
  message_header.msg_control = control_buffer.get();
  message_header.msg_controllen = CMSG_SPACE( sizeof( int ) );

  if ( SystemCall( "recvmsg", recvmsg( fd_num(), &message_header, 0 ) ) ) {
    throw runtime_error( "recv_fd: recvmsg unexpectedly received data" );
  }

  if ( message_header.msg_flags & MSG_CTRUNC ) {
    throw runtime_error( "recvmsg: control data was truncated" );
  }

  const cmsghdr * const control_message = CMSG_FIRSTHDR( &message_header );
  if ( ( not control_message ) or
       ( control_message->cmsg_level != SOL_SOCKET ) or
       ( control_message->cmsg_type != SCM_RIGHTS ) ) {
    throw runtime_error( "recvmsg: unexpected control message" );
  }

  if ( control_message->cmsg_len != CMSG_LEN( sizeof( int ) ) ) {
    throw runtime_error( "recvmsg: unexpected control message length" );
  }

  register_read();

  return *reinterpret_cast<int *>( CMSG_DATA( control_message ) );
}
