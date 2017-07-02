//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef FILE_BODY_HPP_INCLUDED
#define FILE_BODY_HPP_INCLUDED

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

struct file_body
{
    using value_type = boost::filesystem::path;

    static std::uint64_t size( value_type const& v );

    class reader;
    class writer;
};

inline std::uint64_t file_body::size( value_type const& v )
{
    boost::system::error_code ec;

    auto n = boost::filesystem::file_size( v, ec );

    return ec? 0: n;
}

class file_body::reader
{
private:

    value_type const& path_;
    FILE * file_ = nullptr;
    std::uint64_t remain_ = 0;
    char buf_[ 4096 ];

public:

    using const_buffers_type = boost::asio::const_buffers_1;

    template<bool isRequest, class Fields>
    reader(beast::http::message<isRequest, file_body, Fields> const& m, beast::error_code& ec);

    ~reader();

    boost::optional<std::pair<const_buffers_type, bool>>
    get(beast::error_code& ec);
};

inline beast::error_code error_code_from_errno()
{
    return beast::error_code{ errno, beast::generic_category() };
}

template<bool isRequest, class Fields>
file_body::reader::reader( beast::http::message<isRequest, file_body, Fields> const& m, beast::error_code& ec )
    : path_( m.body )
{
    file_ = fopen( path_.string().c_str(), "rb" );

    if( !file_ )
    {
        ec = error_code_from_errno();
        return;
    }

    remain_ = boost::filesystem::file_size( path_, ec );

    if( ec )
    {
        remain_ = 0;
    }
}

inline auto file_body::reader::get( beast::error_code& ec ) -> boost::optional<std::pair<const_buffers_type, bool>>
{
    auto const amount = remain_ > sizeof( buf_ )? sizeof( buf_ ): static_cast<std::size_t>( remain_ );

    if( amount == 0 )
    {
        ec = {};
        return boost::none;
    }

    auto const nread = fread( buf_, 1, amount, file_ );

    if( ferror(file_) )
    {
        ec = error_code_from_errno();
        return boost::none;
    }

    if( nread == 0 )
    {
        ec = make_error_code( boost::system::errc::io_error );
        return boost::none;
    }

    if( nread > remain_ )
    {
        remain_ = 0;
    }
    else
    {
        remain_ -= nread;
    }

    ec = {};

    return {{ const_buffers_type{buf_, nread}, remain_ > 0 }};
}

inline file_body::reader::~reader()
{
    if( file_ )
    {
        fclose(file_);
    }
}

class file_body::writer
{
private:

    value_type const& path_;
    FILE* file_ = nullptr;

public:

    template<bool isRequest, class Fields>
    explicit writer( beast::http::message<isRequest, file_body, Fields>& m, boost::optional<std::uint64_t> const& content_length, beast::error_code& ec );

    template<class ConstBufferSequence> void put( ConstBufferSequence const& buffers, beast::error_code& ec );

    void finish( beast::error_code& ec );

    ~writer();
};

template<bool isRequest, class Fields>
file_body::writer::writer(beast::http::message<isRequest, file_body, Fields>& m,  boost::optional<std::uint64_t> const& content_length, beast::error_code& ec )
    : path_( m.body )
{
    boost::ignore_unused(content_length);

    // Attempt to open the file for writing
    file_ = fopen( path_.string().c_str(), "wb" );

    if( !file_ )
    {
        ec = error_code_from_errno();
        return;
    }

    ec = {};
}

template<class ConstBufferSequence> void file_body::writer::put( ConstBufferSequence const& buffers, beast::error_code& ec )
{
    for( boost::asio::const_buffer buffer: buffers )
    {
        fwrite(
            boost::asio::buffer_cast<void const*>(buffer), 1,
            boost::asio::buffer_size(buffer),
            file_ );

        if( ferror(file_) )
        {
            ec = error_code_from_errno();
            return;
        }
    }

    ec = {};
}

inline void file_body::writer::finish( beast::error_code& ec )
{
    fflush( file_ );

    if( ferror(file_) )
    {
        ec = error_code_from_errno();
        return;
    }

    ec = {};
}

inline file_body::writer::~writer()
{
    if(file_)
    {
        fclose(file_);
    }
}

#endif // #ifndef FILE_BODY_HPP_INCLUDED
