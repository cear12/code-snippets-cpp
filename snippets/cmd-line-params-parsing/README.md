# Command-Line Argument Parsing (two styles)

`main`'s `argv` is a plain `char**` (C-style), which is what every
process actually receives from the OS -- but nothing says application
code has to keep working with it that way past the first line. Two
styles shown side by side: parsing `argv` directly with `strcmp` (C
style, but still a completely normal, common way to write this in C++),
and immediately wrapping it in a `std::vector<std::string_view>` for the
rest of the program to work with modern, safer string handling.
