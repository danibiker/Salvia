// Band-limited sound synthesis buffer
// Various changes and hacks for use in Mednafen.

#ifdef __GNUC__
 #define blip_inline inline __attribute__((always_inline))
#else
 #define blip_inline inline
#endif

#include <limits.h>
#include <stdint.h>

// Blip_Buffer 0.4.1
#ifndef BLIP_BUFFER_H
#define BLIP_BUFFER_H

// Internal

// Output samples are 16-bit signed, with a range of -32768 to 32767
#define BLIP_SAMPLE_MAX 32767

class Blip_Buffer {
public:
	// Set output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second), then clear buffer. Returns 0 on success, otherwise if there
	// isn't enough memory, returns -1.
	int set_sample_rate( long samples_per_sec, int msec_length = 1000 / 4 );
	
	// Set number of source time units per second
	void clock_rate( long );
	
	// End current time frame of specified duration and make its samples available
	// (along with any still-unread samples) for reading with read_samples(). Begins
	// a new time frame at the end of the current frame.
	void end_frame( int32_t time );
	
	// Read at most 'max_samples' out of buffer into 'dest', removing them from from
	// the buffer. Returns number of samples actually read and removed. If stereo is
	// true, increments 'dest' one extra time after writing each sample, to allow
	// easy interleving of two channels into a stereo output buffer.
	long read_samples( int16_t* dest, long max_samples);
	
// Additional optional features

	// Current output sample rate
	long sample_rate() const;
	
	// Length of buffer, in milliseconds
	int length() const;
	
	// Number of source time units per second
	long clock_rate() const;
	
	// Set frequency high-pass filter frequency, where higher values reduce bass more
	void bass_freq( int frequency );
	
	// Remove all available samples and clear buffer to silence. If 'entire_buffer' is
	// false, just clears out any samples waiting rather than the entire buffer.
	void clear( int entire_buffer = 1 );
	
	// Number of samples available for reading with read_samples()
	long samples_avail() const;
	
	// Remove 'count' samples from those waiting to be read
	void remove_samples( long count );
	
// Experimental features
	
	// not documented yet
	uint64_t clock_rate_factor( long clock_rate ) const;
public:
	Blip_Buffer();
	~Blip_Buffer();
private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
public:
	uint64_t factor_;
	uint64_t offset_;
	int32_t *buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
	int bass_shift_;
private:
	long clock_rate_;
	int bass_freq_;
	int length_;
	friend class Blip_Reader;
};

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#define BLIP_BUFFER_ACCURACY 32
#define BLIP_PHASE_BITS 8

	// Internal
	int const blip_widest_impulse_ = 16;
	int const blip_buffer_extra_ = blip_widest_impulse_ + 2;
	int const blip_res = 1 << BLIP_PHASE_BITS;
	class blip_eq_t;
	
	class Blip_Synth_Fast_ {
	public:
		Blip_Buffer* buf;
		int last_amp;
		int delta_factor;
		
		void volume_unit( double );
		Blip_Synth_Fast_();
		void treble_eq( blip_eq_t const& ) { }
	};
	
	class Blip_Synth_ {
	public:
		Blip_Buffer* buf;
		int last_amp;
		int delta_factor;
		
		void volume_unit( double );
		Blip_Synth_( short* impulses, int width );
		void treble_eq( blip_eq_t const& );
	private:
		double volume_unit_;
		short* const impulses;
		int const width;
		int32_t kernel_unit;
	};

// Quality level. Start with blip_good_quality.
const int blip_med_quality  = 8;
const int blip_good_quality = 12;
const int blip_high_quality = 16;

// Range specifies the greatest expected change in amplitude. Calculate it
// by finding the difference between the maximum and minimum expected
// amplitudes (max - min).
template<int quality,int range>
class Blip_Synth {
public:
	// Set overall volume of waveform
	void volume( double v ) { impl.volume_unit( v * (1.0 / (range < 0 ? -range : range)) ); }
	
	// Configure low-pass filter (see blip_buffer.txt)
	void treble_eq( blip_eq_t const& eq )       { impl.treble_eq( eq ); }
	
	// Get/set Blip_Buffer used for output
	Blip_Buffer* output() const                 { return impl.buf; }
	void output( Blip_Buffer* b )               { impl.buf = b; impl.last_amp = 0; }
	
	// Update amplitude of waveform at given time. Using this requires a separate
	// Blip_Synth for each waveform.
	void update( int32_t time, int amplitude );

// Low-level interface

	// Add an amplitude transition of specified delta, optionally into specified buffer
	// rather than the one set with output(). Delta can be positive or negative.
	// The actual change in amplitude is delta * (volume / range)
	void offset( int32_t, int delta, Blip_Buffer* ) const;
	void offset( int32_t t, int delta ) const { offset( t, delta, impl.buf ); }
	
	// Works directly in terms of fractional output samples. Contact author for more info.
	void offset_resampled( uint64_t, int delta, Blip_Buffer* ) const;
	
	// Same as offset(), except code is inlined for higher performance
	void offset_inline( int32_t t, int delta, Blip_Buffer* buf ) const {
		offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
	}
	void offset_inline( int32_t t, int delta ) const {
		offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
	}
	
private:
	Blip_Synth_Fast_ impl;
};

// Low-pass equalization parameters
class blip_eq_t {
public:
	// Logarithmic rolloff to treble dB at half sampling rate. Negative values reduce
	// treble, small positive values (0 to 5.0) increase treble.
	blip_eq_t( double treble_db = 0 );
	
	// See blip_buffer.txt
	blip_eq_t( double treble, long rolloff_freq, long sample_rate, long cutoff_freq = 0 );
	
private:
	double treble;
	long rolloff_freq;
	long sample_rate;
	long cutoff_freq;
	void generate( float* out, int count ) const;
	friend class Blip_Synth_;
};

#define BLIP_SAMPLE_BITS 30

#define BLIP_RESTRICT

// Optimized reading from Blip_Buffer, for use in custom sample output

// Begin reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
	const int32_t * BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
	int32_t name##_reader_accum = (blip_buffer).reader_accum_

// Get value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS( blip_buffer ) ((blip_buffer).bass_shift_)

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass control
int const blip_reader_default_bass = 9;

// Current sample
#define BLIP_READER_READ( name )        (name##_reader_accum >> (BLIP_SAMPLE_BITS - 16))

// Current raw sample in full internal resolution
#define BLIP_READER_READ_RAW( name )    (name##_reader_accum)

// Advance to next sample
#define BLIP_READER_NEXT( name, bass ) \
	(void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

// End reading samples from buffer. The number of samples read must now be removed
// using Blip_Buffer::remove_samples().
#define BLIP_READER_END( name, blip_buffer ) \
	(void) ((blip_buffer).reader_accum_ = name##_reader_accum)

// Deprecated; use BLIP_READER macros as follows:
// Blip_Reader r; r.begin( buf ); -> BLIP_READER_BEGIN( r, buf );
// int bass = r.begin( buf )      -> BLIP_READER_BEGIN( r, buf ); int bass = BLIP_READER_BASS( buf );
// r.read()                       -> BLIP_READER_READ( r )
// r.next( bass )                 -> BLIP_READER_NEXT( r, bass )
// r.next()                       -> BLIP_READER_NEXT( r, blip_reader_default_bass )
// r.end( buf )                   -> BLIP_READER_END( r, buf )
class Blip_Reader {
public:
	int begin( Blip_Buffer& );
	int32_t read() const          { return accum >> (BLIP_SAMPLE_BITS - 16); }
	void next( int bass_shift = 9 )         { accum += *buf++ - (accum >> bass_shift); }
	void end( Blip_Buffer& b )              { b.reader_accum_ = accum; }
	
private:
	const int32_t *buf;
	int32_t accum;
};

// End of public interface

template<int quality,int range>
blip_inline void Blip_Synth<quality,range>::offset_resampled( uint64_t time,
		int delta, Blip_Buffer* blip_buf ) const
{
	delta *= impl.delta_factor;
	int32_t *BLIP_RESTRICT buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (blip_res - 1));

	int32_t left = buf [0] + delta;
	
	// Kind of crappy, but doing shift after multiply results in overflow.
	// Alternate way of delaying multiply by delta_factor results in worse
	// sub-sample resolution.
	int32_t right = (delta >> BLIP_PHASE_BITS) * phase;
	left  -= right;
	right += buf [1];
	
	buf [0] = left;
	buf [1] = right;
}

#undef BLIP_FWD
#undef BLIP_REV

template<int quality,int range>
blip_inline void Blip_Synth<quality,range>::offset( int32_t t, int delta, Blip_Buffer* buf ) const
{
	offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
}

template<int quality,int range>
blip_inline void Blip_Synth<quality,range>::update( int32_t t, int amp )
{
	int delta = amp - impl.last_amp;
	impl.last_amp = amp;
	offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
}

blip_inline blip_eq_t::blip_eq_t( double t ) :
		treble( t ), rolloff_freq( 0 ), sample_rate( 44100 ), cutoff_freq( 0 ) { }
blip_inline blip_eq_t::blip_eq_t( double t, long rf, long sr, long cf ) :
		treble( t ), rolloff_freq( rf ), sample_rate( sr ), cutoff_freq( cf ) { }

blip_inline long Blip_Buffer::samples_avail() const  { return (long) (offset_ >> BLIP_BUFFER_ACCURACY); }
blip_inline void Blip_Buffer::clock_rate( long cps ) { factor_ = clock_rate_factor( clock_rate_ = cps ); }

blip_inline int Blip_Reader::begin( Blip_Buffer& blip_buf )
{
	buf = blip_buf.buffer_;
	accum = blip_buf.reader_accum_;
	return blip_buf.bass_shift_;
}

#define BLIP_MAX_LENGTH 0

#endif
