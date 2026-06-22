#include "Autoindex.hpp"
#include "Logger.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <sstream>
#include <vector>
#include <algorithm>

struct DirEntry
{
	std::string	name;
	bool		is_dir;
	off_t		size;
	time_t		mtime;
};

static bool	entry_less(const DirEntry& a, const DirEntry& b)
{
	if (a.is_dir != b.is_dir)
		return a.is_dir > b.is_dir;
	return a.name < b.name;
}

static	std::string format_time(time_t t)
{
	struct tm*	tm_info = localtime(&t);
	char		buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
	return std::string(buf);
}

static	std::string	format_size(off_t bytes)
{
	std::ostringstream oss;
	if (bytes < 1024)
		oss << bytes << " B";
	else if (bytes < 1024 * 1024)
		oss << (bytes / 1024) << " KB";
	else
		oss << (bytes / (1024 * 1024)) << " MB";

	return oss.str();
}

std::string	Autoindex::generate(const std::string& uri_path,
								const std::string& fs_path)
{
	DIR* dir = opendir(fs_path.c_str());
	if (!dir)
	{
		LOG_FILE_W() << "Autoindex: cannot open dir: " << fs_path;
		return "";
	}

	std::vector<DirEntry>	entries;

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == ".")
			continue;

		std::string full_path = fs_path + "/" + name;
		struct stat st;
		if (stat(full_path.c_str(), &st) != 0)
			continue;

		DirEntry de;
		de.name		= name;
		de.is_dir	= S_ISDIR(st.st_mode);
		de.size		= S_ISREG(st.st_mode) ? st.st_size : -1;
		de.mtime	= st.st_mtime;
		entries.push_back(de);
	}
	closedir(dir);

	std::sort(entries.begin(), entries.end(), entry_less);

	std::string	display_path = uri_path;
	if (display_path.empty() || display_path[display_path.size() - 1] != '/')
		display_path += "/";

	std::ostringstream	html;
    html << "<!DOCTYPE html>\n"
         << "<html><head><meta charset=\"UTF-8\">\n"
         << "<title>Index of " << display_path << "</title>\n"
         << "<style>\n"
         << "body{font-family:monospace;background:#1a1a1a;color:#ccc;padding:2rem;}\n"
         << "h1{color:#7ec8e3;border-bottom:1px solid #333;padding-bottom:.5rem;}\n"
         << "table{width:100%;border-collapse:collapse;}\n"
         << "tr:hover{background:#2a2a2a;}\n"
         << "td,th{padding:.4rem .8rem;text-align:left;}\n"
         << "th{color:#888;border-bottom:1px solid #333;}\n"
         << "a{color:#7ec8e3;text-decoration:none;}\n"
         << "a:hover{text-decoration:underline;}\n"
         << ".dir{color:#f0c040;}\n"
         << ".size{text-align:right;color:#888;}\n"
         << "</style></head>\n"
         << "<body>\n"
         << "<h1>Index of " << display_path << "</h1>\n"
         << "<table>\n"
         << "<tr><th>Name</th><th>Last Modified</th><th class=\"size\">Size</th></tr>\n";

	for (size_t i = 0; i < entries.size(); ++i)
	{
		const DirEntry& de	= entries[i];
		std::string	href 	= display_path + de.name;
		std::string	label	= de.name;

		if (de.is_dir)
		{
			href	+= "/";
			label	+= "/";
		}

        html << "<tr>"
             << "<td><a href=\"" << href << "\""
             << (de.is_dir ? " class=\"dir\"" : "") << ">"
             << label << "</a></td>"
             << "<td>" << format_time(de.mtime) << "</td>"
             << "<td class=\"size\">"
             << (de.is_dir ? "-" : format_size(de.size))
             << "</td>"
             << "</tr>\n";
	}
	html << "</table>\n</body></html>\n";

	return html.str();
}
