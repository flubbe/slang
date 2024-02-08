/**
 * slang - a simple scripting language.
 *
 * utility functions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <list>
#include <string>
#include <utility>
#include <vector>

namespace slang::utils
{

/**
 * Try to get the terminal width. If the terminal width cannot be
 * determined, a default of default_terminal_width (80) is returned.
 * This can happen if none of stdin, stdout, stderr is attached to
 * a terminal (not our use-case).
 *
 * @return The terminal width.
 */
std::size_t get_terminal_width();

/**
 * Split a string at a delimiter.
 *
 * @param s The string to split.
 * @param delimiter The delimiter that separates the string's components.
 * @return A list of components of the original string.
 */
std::list<std::string> split(const std::string& s, const std::string& delimiter);

/**
 * Join a vector of strings.
 *
 * @param v The vector of strings.
 * @param separator The seperator to use between the strings.
 * @return A string made of the vector's strings joined together and separated bythe given separator.
 */
std::string join(const std::vector<std::string>& v, const std::string& separator);

/**
 * Insert line breaks between words after at most len characters.
 * Preserves line breaks in the original string.
 *
 * @param s The string to insert line breaks into.
 * @param line_len The line length.
 */
std::list<std::string> wrap_text(const std::string& s, std::size_t line_len);

/**
 * Print help on commands in a two-column layout to stdout.
 *
 * @param info_text An info text to be printed before the command help.
 * @param cmd_help A vector consisting of pairs (command, help_text) to be formatted and printed to stdout.
 */
void print_command_help(const std::string& info_text, const std::vector<std::pair<std::string, std::string>>& cmd_help);

/**
 * Print usage help for a command.
 *
 * @param usage_text The usage (command line invokation) of the command.
 * @param help_text The explaining help text.
 */
void print_usage_help(const std::string& usage_text, const std::string& help_text);

}    // namespace slang::utils