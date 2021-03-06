#include <algorithm>
#include <cassert>
#include <numeric>

#include "poller.hh"
#include "exception.hh"

using namespace std;
using namespace PollerShortNames;

/* Add an event driven action to the poller. */
void Poller::add_action( Poller::Action action )
{
  actions_.push_back( action );
  pollfds_.push_back( {action.fd.fd_num(), 0, 0} );
}

/* Run poll a single step */
Poller::Result Poller::poll( const int & timeout_ms )
{
  assert( pollfds_.size() == actions_.size() );

  /* tell poll whether we care about each fd */
  for ( unsigned int i = 0; i < actions_.size(); i++ ) {
    assert( pollfds_.at( i ).fd == actions_.at( i ).fd.fd_num() );

    pollfds_.at( i ).events =
      ( actions_.at( i ).active and actions_.at( i ).when_interested() )
        ? actions_.at( i ).direction
        : 0;

    /* don't poll in on fds that have had EOF */
    if ( actions_.at( i ).direction == Direction::In and
         actions_.at( i ).fd.eof() ) {
      pollfds_.at( i ).events = 0;
    }
  }

  /* Quit if no member in pollfds_ has a non-zero direction */
  if ( not accumulate(
         pollfds_.begin(), pollfds_.end(), false,
         []( bool acc, pollfd x ) { return acc or x.events; } ) ) {
    return Result::Type::Exit;
  }

  /* Call 'poll' system call */
  if ( 0 == SystemCall(
              "poll", ::poll( &pollfds_[0], pollfds_.size(), timeout_ms ) ) ) {
    return Result::Type::Timeout;
  }

  /* Run actions for filedescriptors that are ready */
  for ( unsigned int i = 0; i < pollfds_.size(); i++ ) {
    if ( pollfds_[i].revents & ( POLLERR | POLLHUP | POLLNVAL ) ) {
      return Result::Type::Exit;
    }

    // we only want to call callback if revents includes the event we asked for
    if ( pollfds_[i].revents & pollfds_[i].events ) {

      const auto count_before = actions_.at( i ).service_count();
      const auto result = actions_.at( i ).callback();

      /* make sure action made some progress */
      if ( count_before == actions_.at( i ).service_count() ) {
        throw runtime_error(
          "Poller: busy wait detected: callback did not read/write fd" );
      }

      switch ( result.result ) {
      case ResultType::Exit:
        return Result( Result::Type::Exit, result.exit_status );
      case ResultType::Cancel:
        actions_.at( i ).active = false;
      case ResultType::Continue:
        break;
      }
    }
  }

  return Result::Type::Success;
}

/* Run the poller in a loop until an an event handler returns 'Exit'. Return
 * the exit status of the result. */
int Poller::loop( void )
{
  while ( true ) {
    const auto poll_result = poll( -1 );
    if ( poll_result.result == Poller::Result::Type::Exit ) {
      return poll_result.exit_status;
    }
  }
}
