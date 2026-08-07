// Blip_Buffer 0.3.0. http://www.slack.net/~ant/nes-emu/

#include "../include/blip/Stereo_Buffer.h"

/* Library Copyright (C) 2004 Shay Green. Blip_Buffer is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.
Stereo_Buffer is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details. You should have received a copy of the GNU General
Public License along with Stereo_Buffer; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

Stereo_Buffer::Stereo_Buffer() {
}

Stereo_Buffer::~Stereo_Buffer() {
}

bool Stereo_Buffer::set_sample_rate( long rate, int msec )
{
	for ( int i = 0; i < BUF_COUNT; i++ ) {
		if ( bufs [i].set_sample_rate( rate, msec ) == -1)
			return false;
	}
	
	return true;
}

void Stereo_Buffer::clock_rate( long rate )
{
	for ( int i = 0; i < BUF_COUNT; i++ )
		bufs [i].clock_rate( rate );
}

void Stereo_Buffer::bass_freq( int bass )
{
	for ( unsigned i = 0; i < BUF_COUNT; i++ )
		bufs [i].bass_freq( bass );
}

void Stereo_Buffer::clear()
{
	stereo_added = false;
	was_stereo = false;
	for ( int i = 0; i < BUF_COUNT; i++ )
		bufs [i].clear();
}

void Stereo_Buffer::end_frame(int32_t clock_count)
{
	for ( unsigned i = 0; i < BUF_COUNT; i++ )
		bufs [i].end_frame( clock_count );
	stereo_added = true;
}



long Stereo_Buffer::read_samples( int16_t *out, long max_samples )
{
	long count = bufs [0].samples_avail();
	if ( count > max_samples / 2 )
		count = max_samples / 2;
	if ( count )
	{
		if ( stereo_added || was_stereo )
		{
			mix_stereo( out, count );
			
			bufs [0].remove_samples( count );
			bufs [1].remove_samples( count );
			bufs [2].remove_samples( count );
		}
		
		// to do: this might miss opportunities for optimization
		if ( !bufs [0].samples_avail() ) {
			was_stereo = stereo_added;
			stereo_added = false;
		}
	}
	
	return count * 2;
}

void Stereo_Buffer::mix_stereo( int16_t *out, long count )
{
	Blip_Reader left; 
	Blip_Reader right; 
	Blip_Reader center;
	
	left.begin( bufs [1] );
	right.begin( bufs [2] );
	int bass = center.begin( bufs [0] );
	
	while ( count-- )
	{
		int c = center.read();
		out [0] = c + left.read();
		out [1] = c + right.read();
		out += 2;
		
		center.next( bass );
		left.next( bass );
		right.next( bass );
	}
	
	center.end( bufs [0] );
	right.end( bufs [2] );
	left.end( bufs [1] );
}
