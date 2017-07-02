# beast_tpc_server

An example HTTP server written with [Beast](https://github.com/vinniefalco/Beast) as part of my Boost review of the library. Uses the thread-per-connection model and is based on [this Beast example](https://github.com/vinniefalco/Beast/blob/v72/example/http-server-threaded/http_server_threaded.cpp).

This is a simplified port of an existing ad-hoc embedded HTTP server, in which the function `http_get_image` is application-provided.
