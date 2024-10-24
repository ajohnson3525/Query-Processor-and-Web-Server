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

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using hw3::QueryProcessor;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
static const char* kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// static
const int HttpServer::kNumThreads = 100;

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task* t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest& req,
                            const string& base_dir,
                            const list<string>& indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string& uri,
                                const string& base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string& uri,
                                 const list<string>& indices);


///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  cout << "  accepting connections..." << endl << endl;
  ThreadPool tp(kNumThreads);
  while (1) {
    HttpServerTask* hst = new HttpServerTask(HttpServer_ThrFn);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;
    if (!socket_.Accept(&hst->client_fd,
                    &hst->c_addr,
                    &hst->c_port,
                    &hst->c_dns,
                    &hst->s_addr,
                    &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.
      // (Will happen when kill command is used to shut down the server.)
      break;
    }
    // The accept succeeded; dispatch it.
    tp.Dispatch(hst);
  }
  return true;
}

static void HttpServer_ThrFn(ThreadPool::Task* t) {
  // Cast back our HttpServerTask structure with all of our new
  // client's information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next
  // request from our current client, then write out our response.  If
  // the client sends a "Connection: close\r\n" header, then shut down
  // the connection -- we're done.
  //
  // Hint: the client can make multiple requests on our single connection,
  // so we should keep the connection open between requests rather than
  // creating/destroying the same connection repeatedly.

  // STEP 1:
  HttpConnection client_connection(hst->client_fd);
  HttpRequest this_request;
  while (1) {
    // get next request
    if (!client_connection.GetNextRequest(&this_request)) {
      break;
    }
    // check for client asking to close the connection
    if (this_request.GetHeaderValue("connection") == "close") {
      break;
    }

    // proccess request
    HttpResponse this_response = ProcessRequest(this_request, hst->base_dir,
                                                *hst->indices);

    // write the response
    if (!client_connection.WriteResponse(this_response)) {
      break;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest& req,
                            const string& base_dir,
                            const list<string>& indices) {
  // Is the user asking for a static file?
  if (req.uri().substr(0, 8) == "/static/") {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices);
}

static HttpResponse ProcessFileRequest(const string& uri,
                                const string& base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string file_name = "";

  // STEP 2:
  URLParser parser;
  parser.Parse(uri);
  file_name += parser.path().substr(8, string::npos);
  string full_file_name = base_dir + "/" + file_name;
  string end_of_base_dir = base_dir;
  int index;
  while ((index = end_of_base_dir.find("/", 0)) != string::npos) {
    end_of_base_dir = end_of_base_dir.substr(index + 1);
  }
  if (!IsPathSafe(end_of_base_dir, full_file_name)) {
    // File path isn't safe
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                    + EscapeHtml(file_name)
                    + "\"</body></html>\n");
    return ret;
  }
  FileReader fr(base_dir, file_name);

  string contents;
  bool read = fr.ReadFile(&contents);
  ret.AppendToBody(contents);

  string suffix = &file_name[file_name.find(".")];
  if (suffix == ".html" || suffix == ".htm") {
    ret.set_content_type("text/html");
  } else if (suffix == ".jpeg" || suffix == ".jpg") {
    ret.set_content_type("image/jpeg");
  } else if (suffix == ".png") {
    ret.set_content_type("image/png");
  } else if (suffix == ".txt") {
    ret.set_content_type("text/plain");
  } else if (suffix == ".js") {
    ret.set_content_type("application/js");
  } else if (suffix == ".css") {
    ret.set_content_type("text/css");
  } else if (suffix == ".xml") {
    ret.set_content_type("text/xml");
  } else if (suffix == ".gif") {
    ret.set_content_type("image/gif");
  }

  ret.set_response_code(200);
  ret.set_protocol("HTTP/1.1");
  ret.set_message("Success");

  return ret;
}

static HttpResponse ProcessQueryRequest(const string& uri,
                                 const list<string>& indices) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  // 1. The 333gle logo and search box/button should be present on the site.
  //
  // 2. If the user had previously typed in a search query, you also need
  //    to display the search results.
  //
  // 3. you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  // 4. Initialize and use hw3::QueryProcessor to process queries with the
  //    search indices.
  //
  // 5. With your results, try figuring out how to hyperlink results to file
  //    contents, like in solution_binaries/http333d. (Hint: Look into HTML
  //    tags!)

  // STEP 3:

  // add 333gle logo and search bar
  ret.AppendToBody(kThreegleStr);

  URLParser parser;
  parser.Parse(uri);
  map<string, string> args = parser.args();
  if (args.size() != 0) {
    map<string, string>::iterator terms_itr = args.find("terms");

    if (terms_itr != args.end()) {
      // a search was made
      // get terms of search from URI
      string query = terms_itr->second;

      // convert to lowercase
      boost::algorithm::to_lower(query);

      // split terms based on " " delimiter
      vector<string> query_vector;
      boost::algorithm::split(query_vector, query, boost::is_any_of(" "),
                              boost::token_compress_on);

      // process query
      QueryProcessor query_processor(indices);
      vector<QueryProcessor::QueryResult> results
            = query_processor.ProcessQuery(query_vector);

      stringstream num_results_stream;
      num_results_stream << results.size();
      string num_results;
      num_results_stream >> num_results;

      ret.AppendToBody("<p><br>\n");
      ret.AppendToBody(num_results);
      ret.AppendToBody(" results found for <b>");
      ret.AppendToBody(query);
      ret.AppendToBody("</b>\n<p>");

      // add hyperlinked search results to body of response
      vector<QueryProcessor::QueryResult>::iterator itr = results.begin();
      ret.AppendToBody("<ul>");
      while (itr != results.end()) {
        ret.AppendToBody("<li> <a href = \"/static/" +
            EscapeHtml(itr->document_name) + "\">" +
            EscapeHtml(itr->document_name) + "</a>");
        ret.AppendToBody(" [");
        stringstream rank_stream;
        rank_stream << itr->rank;
        string rank;
        rank_stream >> rank;
        ret.AppendToBody(rank);
        ret.AppendToBody("]<br>");
        itr++;
      }
      ret.AppendToBody("</ul>\n");
    }
  }  // end if

  // set other response fields
  ret.AppendToBody("</body>\n</html>\n");
  ret.set_content_type("text/html");
  ret.set_response_code(200);
  ret.set_protocol("HTTP/1.1");
  ret.set_message("Success");

  return ret;
}

}  // namespace hw4
