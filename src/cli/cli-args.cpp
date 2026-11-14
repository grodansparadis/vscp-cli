// cli-args.cpp
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifdef _WIN32
#include <pch.h> // windows.h etc. before the VSCP headers
#endif

#include "cli-args.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#include <vscp-cli-version.h>

namespace vscpcli {

ArgParser::ArgParser(const std::string &prog, const std::string &usage, const std::string &description)
  : m_prog(prog)
  , m_usage(usage)
  , m_description(description)
{
}

void
ArgParser::group(const std::string &title)
{
  m_group = title;
}

void
ArgParser::option(const std::string &longName,
                  char shortName,
                  const std::string &valueName,
                  const std::string &help,
                  std::function<bool(const std::string &, std::string &)> handler)
{
  checkUnique(longName, shortName);
  m_opts.push_back({ longName, shortName, valueName.empty() ? "VALUE" : valueName, help, handler, nullptr, m_group });
}

void
ArgParser::flag(const std::string &longName, char shortName, const std::string &help, std::function<void()> handler)
{
  checkUnique(longName, shortName);
  m_opts.push_back({ longName, shortName, "", help, nullptr, handler, m_group });
}

void
ArgParser::checkUnique(const std::string &longName, char shortName) const
{
  // Programming error: two options with the same name
  if (findLong(longName) || findShort(shortName) || ("help" == longName) || ("version" == longName) ||
      ('h' == shortName)) {
    fprintf(stderr, "%s: option --%s / -%c is defined twice\n", m_prog.c_str(), longName.c_str(),
            shortName ? shortName : ' ');
    abort();
  }
}

const ArgParser::Opt *
ArgParser::findLong(const std::string &name) const
{
  for (const auto &o : m_opts) {
    if (o.longName == name) {
      return &o;
    }
  }
  return nullptr;
}

const ArgParser::Opt *
ArgParser::findShort(char c) const
{
  if (!c) {
    return nullptr;
  }
  for (const auto &o : m_opts) {
    if (o.shortName == c) {
      return &o;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// configFileArgs
//
// Arguments stored in the configuration file given with -c/--config:
// "args" for all tools and "<tool name>" for this tool, both arrays of
// strings (numbers are also accepted). The file is read again later for the
// interface settings.
//

bool
ArgParser::configFileArgs(const std::vector<std::string> &cmd, std::vector<std::string> &args, std::string &error)
{
  std::string path;
  for (size_t i = 0; i < cmd.size(); i++) {
    const std::string &a = cmd[i];
    if ("--" == a) {
      break;
    }
    if ((("-c" == a) || ("--config" == a)) && (i + 1 < cmd.size())) {
      path = cmd[i + 1];
    }
    else if (0 == a.compare(0, 9, "--config=")) {
      path = a.substr(9);
    }
    else if ((a.size() > 2) && (0 == a.compare(0, 2, "-c"))) {
      path = a.substr(2);
    }
  }
  if (path.empty()) {
    return true;
  }

  std::ifstream in(path);
  if (!in) {
    error = "Unable to open the configuration file " + path;
    return false;
  }
  nlohmann::json j;
  try {
    in >> j;
  }
  catch (const std::exception &e) {
    error = "Invalid JSON in " + path + ": " + e.what();
    return false;
  }
  if (!j.is_object()) {
    return true; // buildClientConfig reports this
  }

  for (const std::string key : { std::string("args"), m_prog }) {
    if (!j.contains(key)) {
      continue;
    }
    const auto &list = j[key];
    if (!list.is_array()) {
      error = "\"" + key + "\" in " + path + " must be an array of arguments";
      return false;
    }
    for (const auto &item : list) {
      std::string arg;
      if (item.is_string()) {
        arg = item.get<std::string>();
      }
      else if (item.is_number_integer()) {
        arg = std::to_string(item.get<long long>());
      }
      else {
        error = "\"" + key + "\" in " + path + " must hold strings or numbers";
        return false;
      }
      if (("-c" == arg) || ("--config" == arg) || (0 == arg.compare(0, 9, "--config="))) {
        error = "A configuration file can not name another configuration file";
        return false;
      }
      args.push_back(arg);
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// parse
//

bool
ArgParser::parse(int argc, char **argv, std::vector<std::string> &positional, std::string &error, bool &exitNow)
{
  exitNow = false;
  std::vector<std::string> cmd(argv + 1, argv + argc);

  // Arguments from the configuration file come first so the command line
  // can override them. Positional arguments (the command) are taken from
  // the file only when the command line has none.
  std::vector<std::string> fileArgs;
  if (!configFileArgs(cmd, fileArgs, error)) {
    return false;
  }
  std::vector<std::string> filePositional;
  if (!fileArgs.empty()) {
    if (!parseList(fileArgs, filePositional, error, exitNow)) {
      error += " (in the configuration file)";
      return false;
    }
    if (exitNow) {
      return true;
    }
  }
  if (!parseList(cmd, positional, error, exitNow)) {
    return false;
  }
  if (positional.empty()) {
    positional = filePositional;
  }
  return true;
}

bool
ArgParser::parseList(const std::vector<std::string> &argv, std::vector<std::string> &positional, std::string &error, bool &exitNow)
{
  bool onlyPositional = false;
  const int argc      = (int) argv.size();

  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];

    // A lone "-" (stdin) and negative numbers are positional
    if (onlyPositional || arg.size() < 2 || '-' != arg[0] || (('-' != arg[1]) && isdigit((unsigned char) arg[1]))) {
      positional.push_back(arg);
      continue;
    }

    if ("--" == arg) {
      onlyPositional = true;
      continue;
    }

    if (("--help" == arg) || ("-h" == arg) || ("-?" == arg)) {
      printHelp();
      exitNow = true;
      return true;
    }

    if ("--version" == arg) {
      printf("%s %s (VSCP command line tools)\n", m_prog.c_str(), VSCPCLI_VERSION_STR);
      exitNow = true;
      return true;
    }

    const Opt *opt = nullptr;
    std::string value;
    bool hasValue = false;
    std::string shown;

    if ('-' == arg[1]) {
      std::string name = arg.substr(2);
      size_t eq        = name.find('=');
      if (std::string::npos != eq) {
        value    = name.substr(eq + 1);
        name     = name.substr(0, eq);
        hasValue = true;
      }
      opt   = findLong(name);
      shown = "--" + name;
    }
    else {
      opt   = findShort(arg[1]);
      shown = arg.substr(0, 2);
      if (arg.size() > 2) {
        value    = arg.substr(2);
        hasValue = true;
      }
    }

    if (nullptr == opt) {
      error = "Unknown option " + shown + " (see --help)";
      return false;
    }

    if (opt->flagHandler) {
      if (hasValue) {
        // Combined short flags (-vv, -qj): every letter must be a flag
        if ('-' == arg[1]) {
          error = "Option " + shown + " does not take a value";
          return false;
        }
        for (char c : arg.substr(1)) {
          const Opt *f = findShort(c);
          if ((nullptr == f) || !f->flagHandler) {
            error = std::string("Option -") + c + " can not be combined with other short options";
            return false;
          }
        }
        for (char c : arg.substr(1)) {
          findShort(c)->flagHandler();
        }
        continue;
      }
      opt->flagHandler();
      continue;
    }

    if (!hasValue) {
      if (i + 1 >= argc) {
        error = "Option " + shown + " needs a value (" + opt->valueName + ")";
        return false;
      }
      value = argv[++i];
    }

    std::string err;
    if (!opt->handler(value, err)) {
      error = "Invalid value '" + value + "' for " + shown + (err.empty() ? "" : (": " + err));
      return false;
    }
  }

  return true;
}

void
ArgParser::printHelp(void) const
{
  printf("Usage: %s %s\n\n%s\n", m_prog.c_str(), m_usage.c_str(), m_description.c_str());

  std::string group = "\x01";
  for (const auto &o : m_opts) {
    if (o.group != group) {
      group = o.group;
      printf("\n%s:\n", group.empty() ? "Options" : group.c_str());
    }

    std::string names = o.shortName ? (std::string("  -") + o.shortName + ", ") : std::string("      ");
    names += "--" + o.longName;
    if (!o.flagHandler) {
      names += " " + o.valueName;
    }

    // Help text in a column, continued lines indented
    const size_t column = 32;
    printf("%s", names.c_str());
    size_t pos = names.size();
    if (pos >= column) {
      printf("\n");
      pos = 0;
    }
    printf("%*s", (int) (column - pos), "");
    for (char c : o.help) {
      putchar(c);
      if ('\n' == c) {
        printf("%*s", (int) column, "");
      }
    }
    printf("\n");
  }

  printf("\nGeneral:\n");
  printf("  -h, --help                    Show this help and exit\n");
  printf("      --version                 Show the version and exit\n");

  if (!m_epilog.empty()) {
    printf("\n%s\n", m_epilog.c_str());
  }
}

} // namespace vscpcli
