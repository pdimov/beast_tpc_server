//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//

#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS // localtime

#include <beast/core.hpp>
#include <vector>
#include <cstdint>
#include <cstring>
#include <ctime>

static void blend( unsigned char & r0, unsigned char & g0, unsigned char & b0, unsigned char r1, unsigned char g1, unsigned char b1, double k )
{
    r0 = ( 1 - k ) * r0 + k * r1;
    g0 = ( 1 - k ) * g0 + k * g1;
    b0 = ( 1 - k ) * b0 + k * b1;
}

static void get_color( double x, double y, int hr, unsigned char & r, unsigned char & g, unsigned char & b )
{
    x = 1 - ( 1 - x ) * ( 1 - x ) * ( 1 - x );
    y = y * y * y;

    if( hr >= 12 )
    {
        hr = 23 - hr;
    }

    double z = 1 - hr / 11.0;

    unsigned char r0 = 255, g0 = 255, b0 = 64;
    unsigned char r1 = 96, g1 = 96, b1 = 96;

    blend( r0, g0, b0, r1, g1, b1, z );

    unsigned char r2 = 179, g2 = 212, b2 = 252;
    unsigned char r3 = 16, g3 = 16, b3 = 16;

    blend( r2, g2, b2, r3, g3, b3, z );

    blend( r0, g0, b0, r2, g2, b2, x );

    unsigned char r4 = 83, g4 = 238, b4 = 87;
    unsigned char r5 = 32, g5 = 96, b5 = 32;

    blend( r4, g4, b4, r5, g5, b5, z );

    blend( r0, g0, b0, r4, g4, b4, y );

    r = r0; g = g0; b = b0;
}

static void write_uint32( std::uint32_t v, std::uint16_t w[2] )
{
    w[ 0 ] = v & 0xFFFF;
    w[ 1 ] = ( v >> 16 ) & 0xFFFF;
}

void http_get_image( int width, int height, beast::error_code& ec, std::vector<unsigned char> & data )
{
    std::uint16_t bitmap_file_header[ 7 ] =
    {
        19778,	// bfType, 'BM'
        0, 0,	// bfSize
        0,		// bfReserved1
        0,		// bfReserved2
        0, 0	// bfOffBits
    };

    std::uint32_t bitmap_info_header[ 10 ] =
    {
        sizeof( bitmap_info_header ),	// biSize
        width,							// biWidth
        -height,						// biHeight
        1 + (24<<16),					// biPlanes+biBitCount
        0,								// biCompression (BI_RGB)
        0,								// biSizeImage
        0,								// biXPelsPerMeter
        0,								// biYPelsPerMeter
        0,								// biClrUsed
        0								// biClrImportant
    };

    int pitch = (width * 3 + 3) & ~3u;

    data.resize( sizeof( bitmap_file_header ) + sizeof( bitmap_info_header ) + pitch * height );

    write_uint32( data.size(), bitmap_file_header + 1 );
    write_uint32( sizeof( bitmap_file_header ) + sizeof( bitmap_info_header ), bitmap_file_header + 5 );

    std::memcpy( &data[0], bitmap_file_header, sizeof( bitmap_file_header ) );
    std::memcpy( &data[0] + sizeof( bitmap_file_header ), bitmap_info_header, sizeof( bitmap_info_header ) );

    std::time_t t0 = std::time( 0 );
    int hr = std::localtime( &t0 )->tm_hour;

    unsigned char * p = &data[0] + sizeof( bitmap_file_header ) + sizeof( bitmap_info_header );

    for( int i = 0; i < height; ++i )
    {
        int m = i * pitch;

        for( int j = 0; j < width; ++j )
        {
            get_color( static_cast<double>( j ) / ( width - 1 ), static_cast<double>( i ) / ( height - 1 ), hr, p[ m + 3*j + 2 ], p[ m + 3*j + 1 ], p[ m + 3*j + 0 ] );
        }
    }

    ec = {};
}
