/*
 * Copyright ©2023 Chris Thachuk.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Fall Quarter 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char* kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;
static const int kLargeLen = 1024;

bool HttpConnection::GetNextRequest(HttpRequest* const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:

  int read;
  bool contains_header = false;
  int header_end;
  char buf_arr[kLargeLen];
  buf_arr[0] = '\0';
  // use a do while loop for the case that there is already
  // another request in the buffer, but no data to be read
  do {
    if ((header_end = this->buffer_.find(kHeaderEnd)) != string::npos) {
      contains_header = true;
      break;
    }
    read = WrappedRead(this->fd_, (unsigned char*) buf_arr, kLargeLen);
    buf_arr[read] = '\0';
    this->buffer_ += buf_arr;
    buf_arr[0] = '\0';
  } while (read != -1);
  if (contains_header) {
    // parse buffer, leaving everything after kHeaderEnd in there
    string header = this->buffer_.substr(0, header_end + kHeaderEndLen);
    this->buffer_ = this->buffer_.substr(header_end + kHeaderEndLen);
    HttpRequest this_request = ParseRequest(header);
    *request = this_request;
    return true;
  } else {
    return false;
  }

  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse& response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string& request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

  // extract lines using "\r\n" delimiter
  vector<string> lines;
  boost::algorithm::split(lines, request, boost::is_any_of("\r\n"));

  // start iterator for the lines of the request
  vector<string>::iterator lines_itr = lines.begin();

  // parse first line to set uri, then increment iterator
  // format is "GET [URI] [http_protocol]\r\n", so delim with " "
  vector<string> this_line;
  boost::algorithm::split(this_line, *lines_itr, boost::is_any_of(" "));
  lines_itr++;

  // check for valid first line
  // return if not in correct format
  if (this_line.size() != 3 || this_line[0] != "GET") {
    return req;
  }

  // uri is second item in this_line
  string uri = this_line[1];
  // trim whitespace
  boost::algorithm::trim(uri);
  req.set_uri(uri);
  // add headers from the rest of the lines in a loop
  string name;
  string value;
  while (lines_itr != lines.end()) {
    this_line.clear();
    // format is "[headername]: [headervalue]\r\n"
    // so use delimiter ": " to extract name and value
    boost::algorithm::split(this_line, *lines_itr, boost::is_any_of(": "));
    lines_itr++;
    if (this_line.size() != 3) {
      // skip malformed header lines
      continue;
    }

    name = this_line[0];
    value = this_line[2];

    // trim whitespace
    boost::algorithm::trim(name);
    boost::algorithm::trim(value);
    // convert to lowercase
    boost::algorithm::to_lower(name);
    boost::algorithm::to_lower(value);
    req.AddHeader(name, value);
  }

  return req;
}

}  // namespace hw4
