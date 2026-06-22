#include "MultipartParser.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <sstream>

MultipartParser::MultipartParser() {}
MultipartParser::~MultipartParser() {}

// ─────────────────────────────────────────────
// Extract boundary from "multipart/form-data; boundary=----xxx"
// ─────────────────────────────────────────────

std::string	MultipartParser::extract_boundary(const std::string& content_type)
{
	const std::string token = "boundary=";
	size_t	pos = content_type.find(token);
	if (pos == std::string::npos)
		return "";

	std::string boundary = content_type.substr(pos + token.size());

	size_t	end = boundary.find_first_of(" \t\r\n;");
	if (end != std::string::npos)
		boundary = boundary.substr(0, end);

	if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
		boundary = boundary.substr(1, boundary.size() - 2);

	return boundary;
}

// ─────────────────────────────────────────────
// Parser Entry
// ─────────────────────────────────────────────

bool	MultipartParser::parse(const std::string& body, const std::string& boundary)
{
	if (boundary.empty())
	{
		LOG_REQUEST_E() << "MultipartParser: empty boundary";
		return false;
	}

	const std::string	delim		= "--" + boundary;
	const std::string	delim_end	= "--" + boundary + "--";

	size_t pos = 0;
	pos = body.find(delim, pos);
	if (pos == std::string::npos)
	{
        LOG_REQUEST_E() << "MultipartParser: first boundary not found";
		return false;
	}

	while(true)
	{
		if (body.compare(pos, delim_end.size(), delim_end) == 0)
			break;

		pos += delim.size();
		if (body.compare(pos, 2, "\r\n") == 0)
			pos += 2;
		else if (body.compare(pos, 1, "\n") == 0)
			pos += 1;
		else
			break;

		size_t	next = body.find("\r\n" + delim, pos);
		if (next == std::string::npos)
			next = body.find("\n" + delim, pos);
		if (next == std::string::npos)
			break;

		std::string	raw_part = body.substr(pos, next - pos);
		if (!parse_part(raw_part))
            LOG_REQUEST_W() << "MultipartParser: skipping malformed part";

		size_t next_delim = body.find(delim, next);
		pos = next_delim;
	}
	return true;
}

bool	MultipartParser::parse_part(const std::string& raw_part)
{
	size_t split = raw_part.find("\r\n\r\n");

	std::string	header_block, content;
	if (split != std::string::npos)
	{
		header_block	= raw_part.substr(0, split);
		content			= raw_part.substr(split + 4);
	}
	else
	{
		split = raw_part.find("\n\n");
		if (split == std::string::npos)
			return false;
		header_block	= raw_part.substr(0, split);
		content			= raw_part.substr(split + 2);
	}

	MultipartPart part;
	part.body = content;

	std::istringstream iss(header_block);
	std::string 		line;
	while(std::getline(iss, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		size_t	colon_pos = line.find(':');
		if (colon_pos == std::string::npos)
			continue;

		std::string	key		= line.substr(0, colon_pos);
		std::string	value	= line.substr(colon_pos + 1);

		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		key		= trim(key);
		value	= trim(value);

		part.headers[key] = value;

        if (key == "content-disposition")
			parse_disposition(value, part.name, part.filename);
	}

	_parts.push_back(part);
	return (true);
}

void	MultipartParser::parse_disposition(const std::string& value,
										std::string& out_name,
										std::string& out_filename)
{
	{
		size_t	pos = value.find("name=\"");
		if (pos != std::string::npos)
		{
			pos += 6;
			size_t	end = value.find('"', pos);
			if (end != std::string::npos)
				out_name = value.substr(pos, end - pos);
		}
	}
	{
		size_t	pos = value.find("filename=\"");
		if (pos != std::string::npos)
		{
			pos += 10;
			size_t	end = value.find('"', pos);
			if (end != std::string::npos)
				out_filename = value.substr(pos, end - pos);
		}
	}
}

std::string MultipartParser::trim(const std::string& s)
{
	const std::string whitespace = " \t\r\n";

	size_t start = s.find_first_not_of(whitespace);
	if (start == std::string::npos)
		return "";

	size_t end = s.find_last_not_of(whitespace);
	return s.substr(start, end - start + 1);
}

