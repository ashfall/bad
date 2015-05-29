#include <fcntl.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

#include "imp1.hh"

#include "record.hh"

#include "exception.hh"
#include "file.hh"
#include "util.hh"

// Check size_t for a vector is a size_type or larger
static_assert(
  sizeof( std::vector<Record>::size_type ) >= sizeof( Implementation::size_type ),
  "Vector<Record> max size is too small!"
);

/* Construct Imp1 */
Imp1::Imp1(std::string file)
  : data_{ file.c_str(), O_RDONLY }
  , last_{ Record::MIN }
  , size_{ 0 }
{
}

/* Initialization routine */
void Imp1::DoInitialize( void )
{
  return;
}

/* Read a contiguous subset of the file starting from specified position. */
std::vector<Record> Imp1::DoRead( size_type pos, size_type size )
{
  if (pos == 0) {
    throw std::invalid_argument{"pos must be greater than 0"};
  }

  // establish starting record
  Record after = Record::MIN;
  if (last_.offset() + 1 == pos) {
    after = last_;
  }

  std::vector<Record> recs;
  recs.reserve( size );

  // forward scan through all records until we hit the subset we want.
  while ( after.offset() + 1 < pos + size ) {
    after = linear_scan( data_, after );
    // collect records
    if ( after.offset() >= pos ) {
      recs.push_back( after );
    }
  }

  last_ = after;

  return recs;
}

/* Return the the number of records on disk */
Imp1::size_type Imp1::DoSize( void )
{
  if ( size_ == 0 ) {
    linear_scan( data_, Record::MIN);
  }
  return size_;
}

/* Perform a full linear scan to return the next smallest record that occurs
 * after the 'after' record. */
Record Imp1::linear_scan( File & in, const Record & after )
{
  Record min { Record::MAX };
  size_type i;

  for ( i = 1; ; i++ ) {
    Record next{ i, in.read( Record::SIZE ), false };
    if (in.eof()) {
      break;
    }

    int cmp = next.compare(after);
    if ( cmp >= 0 && next < min ) {
      // we check the offset to ensure we don't pick up same key, but can still
      // handle duplicate keys.
      if ( cmp > 0 || after.offset() < next.offset() ) {
        min = next.clone();
      }
    }
  }

  // cache number of records on disk
  size_ = i;

  // rewind the file
  in.rewind();

  return min;
}
