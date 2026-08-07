// Blip_Buffer 0.4.1. http://www.slack.net/~ant/

#include "../include/blip/Blip_Buffer.h"

#include <climits>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Copyright (C) 2003-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

Blip_Buffer::Blip_Buffer()
{
	factor_       = (uint64_t)ULLONG_MAX;
	offset_       = 0;
	buffer_       = 0;
	buffer_size_  = 0;
	reader_accum_ = 0;
	bass_shift_   = 0;
	clock_rate_   = 0;
	bass_freq_    = 16;
	length_       = 0;
}

Blip_Buffer::~Blip_Buffer()
{
   if (buffer_)
      free( buffer_ );
}

void Blip_Buffer::clear( int entire_buffer )
{
	offset_      = 0;
	reader_accum_ = 0;
	if ( buffer_ )
	{
		long count = (entire_buffer ? buffer_size_ : samples_avail());
		memset( buffer_, 0, (count + blip_buffer_extra_) * sizeof (int32_t) );
	}
}

int Blip_Buffer::set_sample_rate( long new_rate, int msec )
{
	// start with maximum length that resampled time can represent
	int64_t new_size = (ULLONG_MAX >> BLIP_BUFFER_ACCURACY) - blip_buffer_extra_ - 64;

	// simple safety check, since code elsewhere may not be safe for sizes approaching (2 ^ 31).
	if(new_size > ((1LL << 30) - 1))
	 new_size = (1LL << 30) - 1;

	if (msec != BLIP_MAX_LENGTH)
	{
		int64_t s = ((int64_t)44100 * (msec + 1) + 999) / 1000;
		if ( s < new_size )
			new_size = s;
	}
	
	if ( buffer_size_ != new_size )
	{
		void* p = realloc( buffer_, (new_size + blip_buffer_extra_) * sizeof *buffer_ );
		if ( !p )
			return -1;

		buffer_ = (int32_t*) p;
	}
	
	buffer_size_ = new_size;
	
	// update things based on the sample rate
	length_      = new_size * 1000 / 44100 - 1;
	if ( clock_rate_ )
		clock_rate( clock_rate_ );
	bass_freq( bass_freq_ );
	
	clear();
	
	return 0; // success
}

uint64_t Blip_Buffer::clock_rate_factor( long rate ) const
{
	double ratio   = (double) 44100 / rate;
	int64_t factor = (int64_t) floor( ratio * (1LL << BLIP_BUFFER_ACCURACY) + 0.5 );
	return (uint64_t) factor;
}

void Blip_Buffer::bass_freq( int freq )
{
	bass_freq_ = freq;
	int shift = 31;
	if ( freq > 0 )
	{
		shift = 13;
		long f = (freq << 16) / 44100;
		while ( (f >>= 1) && --shift ) { }
	}
	bass_shift_ = shift;
}

void Blip_Buffer::end_frame( int32_t t )
{
	offset_ += t * factor_;
}

void Blip_Buffer::remove_samples( long count )
{
	if ( count )
	{
		offset_ -= (uint64_t) count << BLIP_BUFFER_ACCURACY;
		
		// copy remaining samples to beginning and clear old samples
		long remain = samples_avail() + blip_buffer_extra_;
		memmove( buffer_, buffer_ + count, remain * sizeof *buffer_ );
		memset( buffer_ + remain, 0, count * sizeof *buffer_ );
	}
}

// Blip_Synth_

Blip_Synth_Fast_::Blip_Synth_Fast_()
{
	buf = 0;
	last_amp = 0;
	delta_factor = 0;
}

void Blip_Synth_Fast_::volume_unit( double new_unit )
{
	delta_factor = (int)(new_unit * (1L << BLIP_SAMPLE_BITS) + 0.5);
}

long Blip_Buffer::read_samples( int16_t* BLIP_RESTRICT out, long max_samples)
{
	long count = samples_avail();
	if ( count > max_samples )
		count = max_samples;
	
	if ( count )
	{
		int const bass = BLIP_READER_BASS( *this );
		BLIP_READER_BEGIN( reader, *this );
		
		for ( int32_t n = count; n; --n )
		{
			int32_t s = BLIP_READER_READ( reader );
			if ( (int16_t) s != s )
				s = 0x7FFF - (s >> 24);
			*out = (int16_t) s;
			out += 2;
			BLIP_READER_NEXT( reader, bass );
		}
		BLIP_READER_END( reader, *this );
		
		remove_samples( count );
	}
	return count;
}
