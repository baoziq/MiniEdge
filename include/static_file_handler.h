#pragma once
#include "http_parser.h"


#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
const std::string basic_path = "/home/ubuntu/MiniEdge/www/";
std::string handle_static_request(const HttpRequest& request);