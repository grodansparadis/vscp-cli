// cli-args.h
//
// Small command line option parser used by all the VSCP command line tools.
// It works the same on Linux, macOS and Windows (no getopt_long needed).
//
//   --name value   --name=value   -x value   -xvalue
//   --flag         -f
//   --             ends the options, the rest are positional arguments
//
// Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
// SPDX-License-Identifier: MIT
//

#ifndef VSCPCLI_ARGS_H
#define VSCPCLI_ARGS_H

#include <functional>
#include <string>
#include <vector>

namespace vscpcli {

class ArgParser {
public:
  /*!
    @param prog Program name shown in the help text
    @param usage Usage line(s) without the program name
    @param description Short description shown after the usage line
  */
  ArgParser(const std::string &prog, const std::string &usage, const std::string &description);

  /*!
    Start a new group of options in the help text
  */
  void group(const std::string &title);

  /*!
    Option that takes a value. The handler returns false (and can set the
    error text) if the value is invalid.
  */
  void option(const std::string &longName,
              char shortName,
              const std::string &valueName,
              const std::string &help,
              std::function<bool(const std::string &value, std::string &error)> handler);

  /*!
    Option without a value
  */
  void flag(const std::string &longName, char shortName, const std::string &help, std::function<void()> handler);

  /*!
    Text printed after the options in the help text (examples etc.)
  */
  void epilog(const std::string &text) { m_epilog = text; }

  /*!
    Parse the command line. --help and --version are handled here: the text
    is printed and exitNow is set.
    @return true on success, false on a command line error (error is set).
  */
  bool parse(int argc, char **argv, std::vector<std::string> &positional, std::string &error, bool &exitNow);

  void printHelp(void) const;
  const std::string &prog(void) const { return m_prog; }

private:
  struct Opt {
    std::string longName;
    char shortName;
    std::string valueName; // Empty for a flag
    std::string help;
    std::function<bool(const std::string &, std::string &)> handler;
    std::function<void()> flagHandler;
    std::string group;
  };

  void checkUnique(const std::string &longName, char shortName) const;
  const Opt *findLong(const std::string &name) const;
  const Opt *findShort(char c) const;

  std::string m_prog;
  std::string m_usage;
  std::string m_description;
  std::string m_epilog;
  std::string m_group;
  std::vector<Opt> m_opts;
};

} // namespace vscpcli

#endif
