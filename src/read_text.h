/** @file
    tx_tools - read_text, read text files.

    Copyright (C) 2019 by Christian Zuckschwerdt <zany@triq.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INCLUDE_READTEXT_H_
#define INCLUDE_READTEXT_H_

#define READ_CHUNK_SIZE 8192

// helper to get file contents

char const *read_text_fd(int fd, char const *file_hint);

char const *read_text_file(char const *filename);

#endif /* INCLUDE_READTEXT_H_ */
