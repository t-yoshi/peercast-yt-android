#include "sstream.h"
#include "assets.h"

using namespace std;

// ------------------------------------------------------------
static string MIMEType(const string& path)
{
    using namespace str;

    if (contains(path, ".htm"))
    {
        return MIME_HTML;
    }else if (contains(path, ".css"))
    {
        return MIME_CSS;
    }else if (contains(path, ".jpg"))
    {
        return MIME_JPEG;
    }else if (contains(path, ".gif"))
    {
        return MIME_GIF;
    }else if (contains(path, ".png"))
    {
        return MIME_PNG;
    }else if (contains(path, ".js"))
    {
        return MIME_JS;
    }else if (contains(path, ".svg"))
    {
        return "image/svg+xml";
    }else
    {
        return "application/octet-stream";
    }
}

// ------------------------------------------------------------
AssetsController::AssetsController(const std::string& documentRoot)
    : mapper("/assets", documentRoot)
{
}

// --------------------------------------
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static time_t mtime(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1)
        return -1;
    else
        return st.st_mtime;
}

// ------------------------------------------------------------
#include "cgi.h"
HTTPResponse AssetsController::operator()(const HTTPRequest& req, Stream& stream, Host& remoteHost)
{
    auto path = mapper.toLocalFilePath(req.path);

    if (path.empty())
        return HTTPResponse::notFound();

    StringStream mem;
    FileStream   file;

    time_t last_modified = mtime(path.c_str());
    time_t if_modified_since = -1;

    if (req.headers.get("If-Modified-Since").size())
        if_modified_since = cgi::parseHttpDate(req.headers.get("If-Modified-Since"));

    if (last_modified != -1 && if_modified_since != -1)
        if (last_modified <= if_modified_since)
            return HTTPResponse::notModified({{ "Last-Modified", cgi::rfc1123Time(last_modified) }});

    file.openReadOnly(path.c_str());
    file.writeTo(mem, file.length());

    string body = mem.str();
    map<string,string> headers = {
        {"Content-Type",MIMEType(path)},
        {"Content-Length",to_string(body.size())}
    };

    if (last_modified != -1)
        headers["Last-Modified"] = cgi::rfc1123Time(last_modified);

    return HTTPResponse::ok(headers, body);
}
