#ifndef _URIPARSER_H_
#define _URIPARSER_H_

#include <stdlib.h>
#include <string>
#include <vector>
#include <map>

#define URI_STATE_INIT		0
#define URI_STATE_SUCCESS	1
#define URI_STATE_INVALID	2
#define URI_STATE_ERROR		3

/**
 * @file   URIParser.h
 * @brief  URI parser
 */

// RAII: YES
class ParsedURI
{
public:
	char *scheme;
	char *userinfo;
	char *host;
	char *port;
	char *path;
	char *query;
	char *fragment;
	int state;
	int error;

	ParsedURI() { init(); }
	virtual ~ParsedURI() { deinit(); }

	//copy constructor
	ParsedURI(const ParsedURI& uri) { copy(uri); }
	//copy operator
	ParsedURI& operator= (const ParsedURI& uri)
	{
		if (this != &uri)
		{
			deinit();
			copy(uri);
		}

		return *this;
	}

	//move constructor
	ParsedURI(ParsedURI&& uri);
	//move operator
	ParsedURI& operator= (ParsedURI&& uri);

private:
	void init()
	{
		scheme = NULL;
		userinfo = NULL;
		host = NULL;
		port = NULL;
		path = NULL;
		query = NULL;
		fragment = NULL;
		state = URI_STATE_INIT;
		error = 0;
	}

	void deinit()
	{
		free(scheme);
		free(userinfo);
		free(host);
		free(port);
		free(path);
		free(query);
		free(fragment);
	}

	void copy(const ParsedURI& uri);
};

// static class
class URIParser
{
public:
	// return 0 mean succ, -1 mean fail
	static int parse(const char *str, ParsedURI& uri);
	static int parse(const std::string& str, ParsedURI& uri)
	{
		return parse(str.c_str(), uri);
	}

	static std::map<std::string, std::vector<std::string>>
	split_query_strict(const std::string &query);

	static std::map<std::string, std::string>
	split_query(const std::string &query);

	static std::vector<std::string> split_path(const std::string &path);
};

#endif

