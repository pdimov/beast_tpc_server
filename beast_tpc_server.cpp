//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//

#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS // fopen

#include "file_body.hpp"
#include "../example/common/mime_types.hpp"
#include <beast/core.hpp>
#include <beast/http.hpp>
#include <beast/version.hpp>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <thread>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdio>

namespace ip = boost::asio::ip;
using tcp = boost::asio::ip::tcp;
namespace http = beast::http;
namespace fs = boost::filesystem;

extern void http_get_image( int width, int height, beast::error_code& ec, std::vector<unsigned char> & data );

class connection: public std::enable_shared_from_this<connection>
{
private:

    static int next_id_;

    tcp::socket socket_;
    std::string root_;
    int id_ = ++next_id_;

private:

    static void log_( std::string const & line )
    {
        std::fputs( line.c_str(), stderr );
        std::fflush( stderr );
    }

    template<class A1> void log( A1&& a1 )
    {
        std::ostringstream os;

        os << "[#" << id_ << ' ' << socket_.remote_endpoint() << "] " << a1 << std::endl;

        log_( os.str() );
    }

    template<class A1, class A2> void log( A1&& a1, A2&& a2 )
    {
        std::ostringstream os;

        os << "[#" << id_ << ' ' << socket_.remote_endpoint() << "] " << a1 << ' ' << a2 << std::endl;

        log_( os.str() );
    }

public:

    explicit connection( tcp::socket&& socket, std::string const& root ): socket_( std::move(socket) ), root_( root )
    {
        log( "Connected" );
    }

    void run()
    {
        std::thread( &connection::do_run, shared_from_this() ).detach();
    }

private:

    void error_response( http::status result, std::string const& text, beast::error_code& ec )
    {
        log( static_cast<unsigned>( result ), text );

        http::response<http::string_body> res;

        res.result( result );

        res.set( http::field::server, BEAST_VERSION_STRING );
        res.set( http::field::content_type, "text/plain" );

        res.body = text + "\r\n";

        res.prepare_payload();

        http::write( socket_, res, ec );
    }

    void serve_image( int width, int height, std::string const& rqpath, beast::error_code& ec )
    {
        std::vector<unsigned char> data;

        http_get_image( width, height, ec, data );

        if( ec )
        {
            error_response( http::status::internal_server_error, "'" + rqpath + "': " + ec.message(), ec );
            return;
        }

        beast::string_view sv( (char*)data.data(), data.size() );
        http::response<http::string_view_body> res( sv );

        res.result( http::status::ok );

        res.set( http::field::server, BEAST_VERSION_STRING );
        res.set( http::field::content_type, "image/bmp" );
        res.set( http::field::content_length, data.size() );

        http::write( socket_, res, ec );
    }

    void serve_file( std::string rqpath, beast::error_code& ec )
    {
        if( rqpath.empty() || rqpath[0] != '/' || rqpath.find( ".." ) != std::string::npos )
        {
            error_response( http::status::bad_request, "'" + rqpath + "': bad path", ec );
            return;
        }

        if( rqpath == "/" )
        {
            rqpath += "index.html";
        }

        fs::path path( root_ + rqpath );
        
        if( !fs::exists( path ) )
        {
            error_response( http::status::not_found, "'" + rqpath +  "': not found", ec );
            return;
        }

        auto length = fs::file_size( path, ec );

        if( ec )
        {
            error_response( http::status::not_found, "'" + rqpath +  "': " + ec.message(), ec );
            return;
        }

        http::response<file_body> res( path );

        res.result( http::status::ok );

        res.set( http::field::server, BEAST_VERSION_STRING );
        res.set( http::field::content_type, mime_type( rqpath ) );
        res.set( http::field::content_length, length );

        http::write( socket_, res, ec );
    }

    void do_request( http::request<http::string_body> const & req, beast::error_code& ec )
    {
        log( req.method_string(), req.target() );

        std::string rqpath;
        std::string query;

        {
            std::string target = req.target().to_string();

            std::size_t i = target.find( '?' );

            if( i != std::string::npos )
            {
                rqpath = target.substr( 0, i );
                query = target.substr( i+1 );
            }
            else
            {
                rqpath = target;
            }
        }

        int width = 0, height = 0;

        if( std::sscanf( rqpath.c_str(), "/%d,%d", &width, &height ) == 2 )
        {
            if( width <= 0 || width > 2048 || height <= 0 || height > 2048 )
            {
                error_response( http::status::bad_request, "'" + rqpath + "': bad image size", ec );
            }
            else
            {
                serve_image( width, height, rqpath, ec );
            }
        }
        else
        {
            serve_file( rqpath, ec );
        }
    }

    void do_run()
    {
        try
        {
            beast::error_code ec;
            beast::flat_buffer buffer;

            for( ;; )
            {
                http::request_parser<http::string_body> parser;

                parser.header_limit( 8192 );
                parser.body_limit( 8192 );

                http::read( socket_, buffer, parser, ec );

                if( ec == http::error::end_of_stream )
                {
                    log( "Disconnected" );
                    break;
                }

                if( ec )
                {
                    log( "Read error:", ec.message() );
                    break;
                }

                do_request( parser.get(), ec );

                if( ec )
                {
                    log( "Write error:", ec.message() );
                }
                else
                {
                    log( "200 OK" );
                }
            }

            socket_.shutdown( tcp::socket::shutdown_both, ec );

            if( ec && ec != boost::asio::error::not_connected )
            {
                log( "Shutdown error:", ec.message() );
            }
        }
        catch( std::exception const & e )
        {
            log( "Exception:", e.what() );
        }
    }
};

int connection::next_id_ = 0;

int main()
{
    try
    {
        auto address = tcp::v4();
        unsigned short port = 8001;
        std::string root = "httpdocs";

        boost::asio::io_service ios;

        tcp::acceptor acceptor{ ios, { address, port } };

        for( ;; )
        {
            tcp::socket socket{ ios };

            acceptor.accept( socket );

            std::make_shared<connection>( std::move(socket), root )->run();
        }
    }
    catch( const std::exception& e )
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
